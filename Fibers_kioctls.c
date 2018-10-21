#include <linux/slab.h>

//for pt_regs
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
//for fpu
#include <asm/fpu/internal.h>

#include "Fibers_kioctls.h"

/*
 * To save fpu state try with:
 * copy_fxregs_to_kernel(sstruct fpu *fpu)
 * To restore fpu state use: 
 * copy_kernel_to_fxregs(struct fxregs_state *fx)
 * */

struct fiber_struct* allocate_fiber(pid_t fiber_id, struct task_struct *p, void* (entry_point)(void*), void* args, void* stack_base) 
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
        copy_fxregs_to_kernel(&(fiber->fpu_regs));

        //fiber created by CreateFiber()
        if (entry_point != NULL) 
        {
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
    struct hlist_node       *cursor;
    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    hash_for_each_possible(hashtable->htable, process, next, tgid) 
    {
        if (process->tgid == tgid) //Needed in case of collision
        { 
            printk(KERN_NOTICE "%s:ConvertThreadToFiber() Process %d is activated\n", KBUILD_MODNAME, tgid);
            hlist_for_each(cursor, &process->running_fibers) 
            {
                fiber = hlist_entry(cursor, struct fiber_struct, next);
                if (fiber->parent_thread == pid) 
                {
                    printk(KERN_NOTICE "%s:ConvertThreadToFiber() Thread %d is alrady a fiber\n", KBUILD_MODNAME, tgid);
                    return -ENOTTY;
                }
            }
            goto ALLOCATE_FIBER;
        }
    }

    printk(KERN_NOTICE "%s: ConvertThreadToFiber() Thread %d No active process found\n", KBUILD_MODNAME, pid); 
    
    //New struct for process info
    process = (struct process_active *) kmalloc(sizeof(struct process_active), GFP_KERNEL);
    process->tgid = tgid;
    process->next_fid = 1;
    INIT_HLIST_HEAD(&process->running_fibers);
    INIT_HLIST_HEAD(&process->waiting_fibers);
    INIT_HLIST_NODE(&process->next);
    hash_add(hashtable->htable, &process->next, tgid);

    //FLS
    process->fls = NULL;
    
    printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: new process info allocated\n", KBUILD_MODNAME, pid);

ALLOCATE_FIBER:
    fiber = allocate_fiber(process->next_fid++, current, NULL, NULL, NULL);
    hlist_add_head(&(fiber->next), &(process->running_fibers));
  
    if (copy_to_user((void *) arg, (void *) &fiber->fiber_id,sizeof(fiber_t))) 
    {
        printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return fiber id\n", KBUILD_MODNAME);
        hlist_del(&fiber->next);
        kfree(fiber);
        //Process info can stay
        return -EFAULT;
    }
 
    printk(KERN_NOTICE "%s: Thread %d, ConvertThreadToFiber() was succesful, exiting...\n", KBUILD_MODNAME, pid); 
    return 0;
}

/*
 *  CreateFiber()
 * */

long _ioctl_create(struct module_hashtable *hashtable, struct fiber_args *args)
{
    struct process_active   *process;
    struct fiber_struct     *fiber;

    struct fiber_args       *usr_buf;   //buffer to communicate with usersace

    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    hash_for_each_possible(hashtable->htable, process, next, tgid) 
    {
        if (process->tgid == tgid) 
        {
            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, process found\n", KBUILD_MODNAME, pid);
            usr_buf = (struct fiber_args *) kmalloc(sizeof(struct fiber_args), GFP_KERNEL);
        
            if (copy_from_user((void *) usr_buf, (void *) args, sizeof(struct fiber_args)))
            {
                printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Cannot copy args\n", KBUILD_MODNAME, pid);
                kfree(usr_buf);
                return -EFAULT;
            }
        
            fiber = allocate_fiber(process->next_fid++, current, usr_buf->routine, usr_buf->routine_args, usr_buf->stack_address);
            hlist_add_head(&fiber->next, &process->waiting_fibers);

            usr_buf->ret = fiber->fiber_id;

            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Returning to user fiber_if %d\n", KBUILD_MODNAME, pid, fiber->fiber_id);

            if (copy_to_user((void *) args, (void *) usr_buf, sizeof(struct fiber_args))) 
            {
                printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Cannot Return To User\n", KBUILD_MODNAME, pid);
                hlist_del(&fiber->next);
                kfree(fiber);
                kfree(usr_buf);
                return -EFAULT;
            }

            kfree(usr_buf);
            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Exiting succesfully\n", KBUILD_MODNAME, pid);
            return 0;
        }
    }

    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d cannot be executed sice it is not a Fiber\n", KBUILD_MODNAME, pid);
    return -ENOTTY;

}

long _ioctl_switch(struct module_hashtable *hashtable, fiber_t* usr_id_next) 
{
    struct fiber_struct     *switch_prev;
    struct fiber_struct     *switch_next;
    struct fiber_struct     *cursor;

    struct process_active   *process;
    struct hlist_node       *list_cursor;

    fiber_t                 *id_next;

    pid_t tgid; 
    pid_t pid;
    
    switch_prev = NULL;
    switch_next = NULL;
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d of process %d\n", KBUILD_MODNAME, pid, tgid);

    id_next = (fiber_t *) kmalloc(sizeof(fiber_t), GFP_KERNEL);

    if (copy_from_user((void *) id_next, (void *) usr_id_next, sizeof(fiber_t))) 
    {
        printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d Cannot copy input data\n", KBUILD_MODNAME, pid);
        kfree(id_next);
        return -EFAULT;
    }

    printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, requested: %d\n", KBUILD_MODNAME, pid, *id_next);

    hash_for_each_possible(hashtable->htable, process, next, tgid) 
    {
        if (process->tgid == tgid) 
        {
            hlist_for_each(list_cursor, &process->waiting_fibers) 
            {
                cursor = hlist_entry(list_cursor, struct fiber_struct, next);
                
                printk(KERN_NOTICE "%s: SwitchToFiber() WAITING span %d\n", KBUILD_MODNAME, cursor->fiber_id);
                
                if (cursor->fiber_id == *id_next) 
                {
                    switch_next = cursor;
                }
            }
        
            hlist_for_each(list_cursor, &process->running_fibers) 
            {
                cursor = hlist_entry(list_cursor, struct fiber_struct, next);

                printk(KERN_NOTICE "%s: SwitchToFiber() RUNNING span %d\n", KBUILD_MODNAME, cursor->fiber_id);

                if (cursor->fiber_id == *id_next) 
                {
                    printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Trying to switch to an active fiber\n", KBUILD_MODNAME, pid);
                    cursor->failed_activations += 1;
                    return -ENOTTY;
                }
                else if (cursor->thread_on == pid) 
                {
                    switch_prev = cursor;
                    if (switch_next != NULL) 
                    {
                        //do context switch
                        preempt_disable();
                        printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Switching from %d to %d\n", KBUILD_MODNAME, pid, switch_prev->fiber_id, switch_next->fiber_id);                               
                        copy_fxregs_to_kernel(&(switch_prev->fpu_regs));
                        (void) memcpy(&(switch_prev->regs), task_pt_regs(current), sizeof(struct pt_regs));
                        (void) memcpy(task_pt_regs(current), &(switch_next->regs), sizeof(struct pt_regs));
                        copy_kernel_to_fxregs(&(switch_next->fpu_regs.state.fxsave));
                        preempt_enable();

                        switch_next->activations += 1;
                        switch_next->status = FIBER_RUNNING;
                        switch_prev->status = FIBER_WAITING;

                        hlist_del(&(switch_next->next));
                        hlist_del(&(switch_prev->next));

                        hlist_add_head(&(switch_prev->next), &(process->waiting_fibers));
                        hlist_add_head(&(switch_next->next), &(process->running_fibers));
                        
                        return 0;

                    }
                }
            }
        }
    }

    printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Failed, since not running in fiber context\n", KBUILD_MODNAME, pid);
    return -ENOTTY;

}

long _ioctl_alloc(struct module_hashtable *hashtable, long* arg)
{   
    struct process_active   *process;
    struct fls_struct       *storage;
    
    unsigned long last_bit;
    unsigned long index;

    unsigned long new_size; 
    void* tmp;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    printk(KERN_NOTICE "%s: FLSAlloc() called by thread %d of process %d\n", KBUILD_MODNAME, pid, tgid);

    hash_for_each_possible(hashtable->htable, process, next, tgid) 
    {
        if (process->tgid == tgid) 
        {
            if (process->fls == NULL) 
            {
                printk(KERN_NOTICE "%s: FLSAlloc() No FLS for process %d\n", KBUILD_MODNAME, tgid);
                process->fls = kzalloc(sizeof(struct fls_struct), GFP_KERNEL);
                process->fls->current_size = process->next_fid < MAX_FLS_INDEX?process->next_fid : MAX_FLS_INDEX;
                process->fls->fls = kvzalloc(sizeof(long long)* process->fls->current_size , GFP_KERNEL);
                process->fls->size = 0;
                process->fls->used_index = kvzalloc(sizeof(unsigned long)*BITS_TO_LONGS(process->fls->current_size),GFP_KERNEL);
            }
            storage = process->fls;
            if (storage->size >= storage->current_size) 
            {   
                printk(KERN_NOTICE "%s: FLSAlloc() resizing FLS for process %d\n", KBUILD_MODNAME, tgid);
                new_size = storage->current_size * 2 < MAX_FLS_INDEX? storage->current_size*2:MAX_FLS_INDEX;
                last_bit = BITS_TO_LONGS(new_size);
                
                tmp = krealloc(storage->fls, sizeof(long long)*new_size, GFP_KERNEL);
                if (!tmp)
                    return -ENOMEM;
                storage->fls = tmp;
                tmp = krealloc(storage->used_index, sizeof(unsigned long)*last_bit, GFP_KERNEL);
                if (!tmp)
                    return -ENOMEM;
                storage->used_index = tmp;

                storage->current_size = new_size;
                
            }

            last_bit = storage->current_size;
            printk(KERN_NOTICE "%s: FLSAlloc() last bit is %ld, for process %d\n", KBUILD_MODNAME, last_bit, tgid); 

            index = find_first_zero_bit(storage->used_index, last_bit);   
            
            printk(KERN_NOTICE "%s: FLSAlloc() FLS index is %ld, for process %d\n", KBUILD_MODNAME, index,tgid); 

            if (index < last_bit) 
            {
                storage->size += 1;
                set_bit(index, storage->used_index);

                printk(KERN_NOTICE "%s: FLSAlloc() retruning FLS index %ld, for process %d\n", KBUILD_MODNAME, index,tgid); 

                if (copy_to_user((void *) arg, (void *) &index, sizeof(long)))
                {
                    printk(KERN_NOTICE "%s:  FlsAlloc() cannot return index\n", KBUILD_MODNAME);
                    clear_bit(index, storage->used_index);
                    return -EFAULT;
                } 
                return 0;
            }
            else
                return -ENOTTY;
        }
    }

    return -ENOTTY;

}

long _ioctl_free(struct module_hashtable *hashtable, long* index) {}