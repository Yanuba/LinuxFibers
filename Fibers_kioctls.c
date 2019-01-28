#include <linux/slab.h>
//for pt_regs
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
//for fpu
#include <asm/fpu/internal.h>

#include "Fibers_kioctls.h"

/*
 * Function to find process in hashtable -> convert it into a macro
 * */
inline struct process_active* find_process(struct module_hashtable *hashtable, pid_t tgid)
{
    struct process_active *ret;
    hash_for_each_possible(hashtable->htable, ret, next, tgid)
    {
        if (ret->tgid == tgid)
            return ret;
    }
    return NULL;
}

struct fiber_struct* allocate_fiber(pid_t fiber_id, struct task_struct *p, void (*entry_point)(void*), void* args, void* stack_base) 
{
        struct fiber_struct* fiber;

        fiber = kzalloc(sizeof(struct fiber_struct), GFP_KERNEL);
        fiber->fiber_id = fiber_id;
        fiber->thread_on = fiber->parent_thread = task_pid_nr(p);
        fiber->parent_process = task_tgid_nr(p);
        fiber->activations = 1;
        fiber->failed_activations = 0;
        fiber->status = FIBER_RUNNING;
        fiber->execution_time = 0;
        INIT_HLIST_NODE(&fiber->next);

        (void) memcpy(&(fiber->regs), task_pt_regs(p), sizeof(struct pt_regs));
        copy_fxregs_to_kernel(&(fiber->fpu_regs)); //save FPU state

        //fiber created by CreateFiber()
        if (entry_point != NULL) 
        {   
            fiber->activations = 0;
            fiber->status = FIBER_WAITING;
            fiber->regs.ip = (unsigned long) entry_point;
            fiber->regs.di = (unsigned long) args;
            fiber->regs.sp = fiber->regs.bp = (unsigned long) stack_base;
        }

        fiber->entry_point = (void *) fiber->regs.ip;

        return fiber;
}

/*
IOCTL
ERRORS
       EBADF  fd is not a valid file descriptor.

       EFAULT argp references an inaccessible memory area.

       EINVAL request or argp is not valid.

       ENOTTY fd is not associated with a character special device.

       ENOTTY The specified request does not apply to the kind of object  that
              the file descriptor fd references.
*/

/*
 *  ConvertTreadtoFiber()
 * */

long _ioctl_convert(struct module_hashtable *hashtable, fiber_t* arg)
{    
    struct process_active   *process;
    struct fiber_struct     *fiber;
    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);
    
    printk_msg("[%d, %d] ConvertThreadToFiber() ENTER", tgid, pid);

    process = find_process(hashtable, tgid);
    
    if (process) 
    {   
        printk_msg("[%d, %d] ConvertThreadToFiber() Process Active", tgid, pid);
        hlist_for_each_entry(fiber, &process->running_fibers, next)
        {
            if (fiber->parent_thread == pid) 
            {   
                printk_msg("[%d, %d] ConvertThreadToFiber() Thread already fiber, EXIT", tgid, pid);
                return -ENOTTY;
            }
        }
        
        goto ALLOCATE_FIBER;
    }

    //New struct for process info
    process = (struct process_active *) kmalloc(sizeof(struct process_active), GFP_KERNEL);
    process->tgid = tgid;
    process->next_fid = 0;
    INIT_HLIST_HEAD(&process->running_fibers);
    INIT_HLIST_HEAD(&process->waiting_fibers);
    INIT_HLIST_NODE(&process->next);

    //FLS
    spin_lock_init(&(process->fls.fls_lock));
    process->fls.size = -1;
    process->fls.fls = NULL;
    process->fls.used_index = NULL;
    
    hash_add(hashtable->htable, &process->next, tgid);

ALLOCATE_FIBER:
    fiber = allocate_fiber(process->next_fid++, current, NULL, NULL, NULL);
    hlist_add_head(&(fiber->next), &(process->running_fibers));
  
    if (copy_to_user((void *) arg, (void *) &fiber->fiber_id, sizeof(fiber_t))) 
    {
        printk_msg("[%d, %d] ConvertThreadToFiber() Cannot return EXIT", tgid, pid);
        hlist_del(&fiber->next);
        kfree(fiber);
        //If the first call of a process fails here, we can create fiber even if we are not in a fiber context
        return -EFAULT;
    }
    
    printk_msg("[%d, %d] ConvertThreadToFiber() EXIT SUCCESS", tgid, pid);
    return 0;
}

/*
 *  CreateFiber()
 * */

long _ioctl_create(struct module_hashtable *hashtable, struct fiber_args *args)
{
    struct process_active   *process;
    struct fiber_struct     *fiber;

    struct fiber_args       usr_buf;   //buffer to communicate with userspace

    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    printk_msg("[%d, %d] CreateFiber() ENTER", tgid, pid);

    process = find_process(hashtable, tgid);
    
    if (!process)
    {
        printk(KERN_NOTICE "%s: CreateFiber() called by thread %d cannot be executed sice it is not a Fiber\n", KBUILD_MODNAME, pid);
        return -ENOTTY;
    }

    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, process found\n", KBUILD_MODNAME, pid);
        
    // check if we are fiber

    if (copy_from_user((void *) &usr_buf, (void *) args, sizeof(struct fiber_args)))
    {
        printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Cannot copy args\n", KBUILD_MODNAME, pid);
        return -EFAULT;
    }
        
    fiber = allocate_fiber(process->next_fid++, current, usr_buf.routine, usr_buf.routine_args, usr_buf.stack_address);
    hlist_add_head(&fiber->next, &process->waiting_fibers);

    usr_buf.ret = fiber->fiber_id;

    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Returning to user fiber_if %d\n", KBUILD_MODNAME, pid, fiber->fiber_id);

    if (copy_to_user((void *) args, (void *) &usr_buf, sizeof(struct fiber_args))) 
    {
        printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Cannot Return To User\n", KBUILD_MODNAME, pid);
        hlist_del(&fiber->next);
        kfree(fiber);
        return -EFAULT;
    }

    printk_msg("[%d, %d] CreateFiber() EXIT SUCCESS", tgid, pid);
    return 0;
    
}

/*
 * Need a review - need strong memory barriers
 * RCU maybe is not suitable since we don't want that in some moment
 * a fiber keep existing as a copy in some other thread
 * */
long _ioctl_switch(struct module_hashtable *hashtable, fiber_t* usr_id_next) 
{
    struct fiber_struct     *switch_prev;
    struct fiber_struct     *switch_next;
    struct fiber_struct     *cursor;
    struct process_active   *process;

    fiber_t                 id_next;

    pid_t tgid; 
    pid_t pid;
    
    switch_prev = NULL;
    switch_next = NULL;
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    printk_msg("[%d, %d] SwitchToFiber() ENTER", tgid, pid);

    if (copy_from_user((void *) &id_next, (void *) usr_id_next, sizeof(fiber_t))) 
    {
        printk_msg("[%d, %d] SwitchToFiber() Cannot Read Input EXIT", tgid, pid);
        return -EFAULT;
    }

    process = find_process(hashtable, tgid);
    
    if (!process) 
    {   
        printk_msg("[%d, %d] SwitchToFiber() Not in Fiber Context EXIT", tgid, pid);
        return -ENOTTY;
    }

    hlist_for_each_entry(cursor, &process->waiting_fibers, next)
    {
        if (cursor->fiber_id == id_next) 
        {
            switch_next = cursor;
            break;
        }
    }
    if (cursor->fiber_id != id_next) 
        switch_next = NULL;

    hlist_for_each_entry(cursor, &process->running_fibers, next) 
    {
        if (cursor->fiber_id == id_next) 
        {   
            printk_msg("[%d, %d] SwitchToFiber() Trying to Switch to an active fiber", tgid, pid);
            cursor->failed_activations += 1;
            return -ENOTTY;
        }
      
        else if (switch_next && cursor->thread_on == pid) 
        {
            switch_prev = cursor;
            
            //do context switch
            printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Switching from %d to %d\n", KBUILD_MODNAME, pid, switch_prev->fiber_id, switch_next->fiber_id);                                 
            preempt_disable();
            copy_fxregs_to_kernel(&(switch_prev->fpu_regs)); //save FPU state
            (void) memcpy(&(switch_prev->regs), task_pt_regs(current), sizeof(struct pt_regs));
            (void) memcpy(task_pt_regs(current), &(switch_next->regs), sizeof(struct pt_regs));
            copy_kernel_to_fxregs(&(switch_next->fpu_regs.state.fxsave)); //restore FPU state
                
            switch_next->activations += 1;
            switch_next->thread_on = pid;
            switch_next->status = FIBER_RUNNING;
            
            switch_prev->status = FIBER_WAITING;
    
            hlist_del(&(switch_next->next));
            hlist_del(&(switch_prev->next));

            hlist_add_head(&(switch_prev->next), &(process->waiting_fibers));
            hlist_add_head(&(switch_next->next), &(process->running_fibers));
            preempt_enable();

            printk_msg("[%d, %d] SwitchToFiber() EXIT SUCCESS", tgid, pid);
            return 0;

        }   
    }

    printk_msg("[%d, %d] SwitchToFiber() No FIbers, EXIT", tgid, pid);
    return -ENOTTY;
}  

//
long _ioctl_alloc(struct module_hashtable *hashtable, long* arg)
{   
    struct process_active   *process;
    struct fls_struct       *storage;

    unsigned long index;
    unsigned long flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    printk_msg("[%d, %d] FLSAlloc() ENTER", tgid, pid);
    
    process = find_process(hashtable, tgid);
    if (!process)
        return -ENOTTY;

    storage = &(process->fls);

    spin_lock_irqsave(&storage->fls_lock, flags);

    if (storage->fls == NULL) 
    {
        printk_msg("[%d, %d] FLSAlloc() No FLS array, EXIT", tgid, pid);
        storage->fls = vmalloc(sizeof(long long)* MAX_FLS_INDEX); //we are asking for 32Kb (8 pages)
        storage->size = 0;
        storage->used_index = kzalloc(sizeof(unsigned long)*BITS_TO_LONGS(MAX_FLS_INDEX), GFP_KERNEL); //1 page
    }

    index = find_first_zero_bit(storage->used_index, MAX_FLS_INDEX);   

    if (index < MAX_FLS_INDEX && storage->size < MAX_FLS_INDEX-1) 
    {
        storage->size += 1;
        set_bit(index, storage->used_index);

        printk_msg("[%d, %d] FLSAlloc(), used index %ld", tgid, pid, index);
        
        if (copy_to_user((void *) arg, (void *) &index, sizeof(long)))
        {
            printk_msg("[%d, %d] FLSAlloc() Cannot Return Index, EXIT", tgid, pid);
            storage->size -= 1;
            clear_bit(index, storage->used_index);
            spin_unlock_irqrestore(&storage->fls_lock, flags);
            return -EFAULT;
        } 
        spin_unlock_irqrestore(&storage->fls_lock, flags);
        printk_msg("[%d, %d] FLSAlloc() EXIT SUCCESS", tgid, pid);
        return 0;
        
    }    
    else
    {
        spin_unlock_irqrestore(&storage->fls_lock, flags);
        printk_msg("[%d, %d] FLSAlloc() EXIT FAILURE", tgid, pid);
        return -ENOTTY;
    }
}

long _ioctl_free(struct module_hashtable *hashtable, long* arg) 
{
    struct process_active   *process;
    struct fls_struct       *storage;

    unsigned long           index;
    unsigned long           flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    printk_msg("[%d, %d] FLSFree() ENTER", tgid, pid);
    
    process = find_process(hashtable, tgid);
    
    if (!process)
        return -ENOTTY;

    if (copy_from_user((void *) &index, (void *) arg, sizeof(long)))
    {
        return -EFAULT;
    }

    if (index > MAX_FLS_INDEX) 
    {   
        printk_msg("[%d, %d] FLSFree() Freeing Invalid index, EXIT", tgid, pid);
        return -ENOTTY;
    }

    storage = &(process->fls);

    spin_lock_irqsave(&storage->fls_lock, flags);
    
    if (storage->fls == NULL || storage->used_index == NULL) {
        printk_msg("[%d, %d] FLSFree() No FLS, EXIT", tgid, pid);
        return -ENOTTY;
    }
    storage->size -= 1;
    clear_bit(index, storage->used_index);
    
    spin_unlock_irqrestore(&storage->fls_lock, flags);
    
    printk_msg("[%d, %d] FLSFree() EXIT SUCCESS", tgid, pid);
    return 0; 
       
}

long _ioctl_get(struct module_hashtable *hashtable, struct fls_args* args) 
{
    struct process_active   *process;
    struct fls_struct       *storage;
    
    struct fls_args           ret;

    unsigned long flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);
    
    printk_msg("[%d, %d] FLSGet() ENTER", tgid, pid);

    process = find_process(hashtable, tgid);
    
    if (!process)
    {
        printk_msg("[%d, %d] FLSGet() No Fiber Context, EXIT", tgid, pid);
        return -ENOTTY;
    }

    if (copy_from_user((void *) &ret, (void *) args, sizeof(struct fls_args)))
    {   
        printk_msg("[%d, %d] FLSGet() Cannot Access input data EXIT", tgid, pid);
        return -EFAULT;
    }

    if ((ret.index) > MAX_FLS_INDEX)
    {   
        printk_msg("[%d, %d] FLSGet() Index Out of Bound, EXIT", tgid, pid);
        return -ENOTTY;
    }

    storage = &(process->fls);
    spin_lock_irqsave(&storage->fls_lock, flags);
    
    if (test_bit(ret.index, storage->used_index))
    {
        ret.value = storage->fls[ret.index];
        printk_msg("[%d, %d] FLSGet(), used index %ld", tgid, pid, ret.index);
        if (copy_to_user((void *) args, (void *) &ret, sizeof(struct fls_args)))
        {   
            spin_unlock_irqrestore(&storage->fls_lock, flags);
            printk_msg("[%d, %d] FLSGet() Cannot Return, EXIT", tgid, pid);
            return -EFAULT;
        }
        spin_unlock_irqrestore(&storage->fls_lock, flags);
        printk_msg("[%d, %d] FLSGet() EXIT SUCCESS", tgid, pid);
        return 0;
    }
    else
    {   
        spin_unlock_irqrestore(&storage->fls_lock, flags);
        printk_msg("[%d, %d] FLSGet() Index Not in Use, EXIT", tgid, pid);
        return -ENOTTY;
    }
}

long _ioctl_set(struct module_hashtable *hashtable, struct fls_args* args) 
{
    struct process_active   *process;
    struct fls_struct       *storage;
    struct fls_args         ret;

    unsigned long flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);
    
    printk_msg("[%d, %d] FLSSet() ENTER", tgid, pid);
    
    process = find_process(hashtable, tgid);
    
    if (!process) 
    {
        printk_msg("[%d, %d] FLSSet() Not in FIber Context EXIT", tgid, pid);
        return -ENOTTY;
    }

    if (copy_from_user((void *) &ret, (void *) args, sizeof(struct fls_args)))
    {   
        printk_msg("[%d, %d] FLSSet() Cannot read Input EXIT", tgid, pid);
        return -EFAULT;
    }

    if ((ret.index) > MAX_FLS_INDEX)
    {   
        printk_msg("[%d, %d] FLSSet() Index Out of Bound EXIT", tgid, pid);
        return -ENOTTY;
    }

    storage = &(process->fls);

    spin_lock_irqsave(&storage->fls_lock, flags);

    if (test_bit(ret.index, storage->used_index))
    {   
        printk_msg("[%d, %d] FLSGet(), used index %ld", tgid, pid, ret.index);
        storage->fls[ret.index] = ret.value;
        spin_unlock_irqrestore(&storage->fls_lock, flags);
        printk_msg("[%d, %d] FLSSet() EXIT SUCCESS", tgid, pid);
        return 0;
    }
    else
    {   
        spin_unlock_irqrestore(&storage->fls_lock, flags);
        printk_msg("[%d, %d] FLSSet() Index Not in Use EXIT", tgid, pid);
        return -ENOTTY;
    }
}

/*
 * Missing Cleanup of the data structures when a fiber exits or when the module is unmounted
 */