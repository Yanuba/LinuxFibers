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

inline struct process_active* allocate_process(pid_t tgid)
{   
    struct process_active* process;

    process = (struct process_active *) kmalloc(sizeof(struct process_active), GFP_KERNEL);
    
    process->tgid = tgid;
    process->next_fid = 0;
    INIT_HLIST_HEAD(&process->running_fibers);
    INIT_HLIST_HEAD(&process->waiting_fibers);
    INIT_HLIST_NODE(&process->next);
    spin_lock_init(&(process->lock));
    
    return process;    
}

inline struct fiber_struct* allocate_fiber(pid_t fiber_id, struct task_struct *p, void (*entry_point)(void*), void* args, void* stack_base) 
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

        //FLS
        //spin_lock_init(&(fiber->fls.fls_lock));
        fiber->fls.size = -1;
        fiber->fls.fls = NULL;
        fiber->fls.used_index = NULL;

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

/**
 *  ConvertTreadtoFiber()
 */
long _ioctl_convert(struct module_hashtable *hashtable, fiber_t* arg)
{    
    struct process_active   *process;
    struct fiber_struct     *fiber;
    struct hlist_head       *run_fib;

    unsigned long           flags;

    pid_t                   fid;

    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);
    
    rcu_read_lock();
    hash_for_each_possible_rcu(hashtable->htable, process, next, tgid)
    {
        if (process->tgid == tgid) {
            run_fib = &process->running_fibers;
            rcu_read_unlock();
            break;
        }
    }
    if (!process || process->tgid != tgid)
    {
        rcu_read_unlock();
        run_fib = NULL;
        goto ALLOCA_PROCESS;
    }

    rcu_read_lock();
    hlist_for_each_entry_rcu(fiber, run_fib, next) {
        if (fiber->thread_on == tgid) 
        {
            rcu_read_unlock();
            return -ENOTTY;
        }
    }
    rcu_read_unlock();
    goto ALLOCA_FIBER;

ALLOCA_PROCESS:
    process = allocate_process(tgid);
    run_fib = &process->running_fibers;

    if (spin_trylock_irqsave(&hashtable->lock, flags))
        hash_add_rcu(hashtable->htable, &process->next, tgid);
    else
    {   
        spin_unlock_irqrestore(&hashtable->lock, flags);
        kfree(process);
        return -EFAULT;
    }
    spin_unlock_irqrestore(&hashtable->lock, flags);

ALLOCA_FIBER:

    spin_lock_irqsave(&process->lock, flags);
    
    fid = process->next_fid++;
    fiber = allocate_fiber(fid, current, NULL, NULL, NULL);
    hlist_add_head_rcu(&(fiber->next), run_fib);
    
    spin_unlock_irqrestore(&process->lock, flags);

    if (copy_to_user((void *) arg, (void *) &fid, sizeof(fiber_t))) 
    {
        spin_lock_irqsave(&process->lock, flags);
        hlist_del(&fiber->next);
        spin_unlock_irqrestore(&process->lock, flags);
        kfree(fiber);
        //If the first call of a process fails here, we can create fiber even if we are not in a fiber context
        return -EFAULT;
    }

    return 0;
}

/**
 *  CreateFiber()
 */
long _ioctl_create(struct module_hashtable *hashtable, struct fiber_args *args)
{
    struct process_active   *process;
    struct fiber_struct     *fiber;
    struct hlist_head       *wait_fib;
    struct hlist_head       *run_fib;

    struct fiber_args       usr_buf;   //buffer to communicate with userspace

    unsigned long           flags;

    pid_t                   fid;

    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    rcu_read_lock();
    hash_for_each_possible_rcu(hashtable->htable, process, next, tgid)
    {
        if (process->tgid == tgid) {
            run_fib = &process->running_fibers;
            wait_fib = &process->waiting_fibers;
            rcu_read_unlock();
            break;
        }
    }
    if (!process || process->tgid != tgid)
    {
        rcu_read_unlock();
        return -EFAULT;
    }

    rcu_read_lock();
    hlist_for_each_entry_rcu(fiber, run_fib, next)
    {
        if (fiber->thread_on == pid)
        {
            rcu_read_unlock();
            break;
        }
    }
    if (!fiber || fiber->thread_on != pid)
    {
        rcu_read_unlock();
        return -ENOTTY;
    }
    
    if (copy_from_user((void *) &usr_buf, (void *) args, sizeof(struct fiber_args)))
        return -EFAULT;
    
    spin_lock_irqsave(&process->lock, flags);
    
    fid = process->next_fid++;
    fiber = allocate_fiber(fid, current, usr_buf.routine, usr_buf.routine_args, usr_buf.stack_address);
    hlist_add_head_rcu(&fiber->next, wait_fib);
    usr_buf.ret = fiber->fiber_id;
    
    spin_unlock_irqrestore(&process->lock, flags);

    if (copy_to_user((void *) args, (void *) &usr_buf, sizeof(struct fiber_args))) 
    {
        spin_lock_irqsave(&process->lock, flags);
        hlist_del_rcu(&fiber->next);
        spin_unlock_irqrestore(&process->lock, flags);
        synchronize_rcu();
        kfree(fiber);
        return -EFAULT;
    }

    return 0;   
}

long _ioctl_switch(struct module_hashtable *hashtable, fiber_t* usr_id_next) 
{
    struct fiber_struct     *switch_prev;
    struct fiber_struct     *switch_next;
    struct fiber_struct     *cursor;
    struct process_active   *process;

    struct hlist_head       *wait_fib;
    struct hlist_head       *run_fib;

    unsigned long           flags;

    fiber_t                 id_next;

    pid_t tgid; 
    pid_t pid;
    
    switch_prev = NULL;
    switch_next = NULL;
    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    if (copy_from_user((void *) &id_next, (void *) usr_id_next, sizeof(fiber_t))) 
        return -EFAULT;

    rcu_read_lock();
    hash_for_each_possible_rcu(hashtable->htable, process, next, tgid)
    {
        if (process->tgid == tgid) {
            run_fib = &process->running_fibers;
            wait_fib = &process->waiting_fibers;
            rcu_read_unlock();
            break;
        }
    }
    if (!process || process->tgid != tgid)
    {
        rcu_read_unlock();
        return -EFAULT;
    }

    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry(cursor, wait_fib, next)
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
            spin_unlock_irqrestore(&process->lock, flags);
            cursor->failed_activations += 1;
            return -ENOTTY;
        }
      
        else if (switch_next && cursor->thread_on == pid) 
        {
            switch_prev = cursor;
            //do context switch
            
            hlist_del_rcu(&(switch_next->next));
            hlist_del_rcu(&(switch_prev->next));

            spin_unlock_irqrestore(&process->lock, flags);
            synchronize_rcu();

            //preempt_disable();
            copy_fxregs_to_kernel(&(switch_prev->fpu_regs)); //save FPU state
            (void) memcpy(&(switch_prev->regs), task_pt_regs(current), sizeof(struct pt_regs));
            (void) memcpy(task_pt_regs(current), &(switch_next->regs), sizeof(struct pt_regs));
            copy_kernel_to_fxregs(&(switch_next->fpu_regs.state.fxsave)); //restore FPU state
            //preempt_enable();

            switch_next->activations += 1;
            switch_next->thread_on = pid;
            switch_next->status = FIBER_RUNNING;
            
            switch_prev->status = FIBER_WAITING;
    
            spin_lock_irqsave(&process->lock, flags);

            hlist_add_head_rcu(&(switch_prev->next), wait_fib);
            hlist_add_head_rcu(&(switch_next->next), run_fib);
            
            spin_unlock_irqrestore(&process->lock, flags);
            return 0;
        }   
    }

    spin_unlock_irqrestore(&process->lock, flags);
    return -ENOTTY;
} 

long _ioctl_alloc(struct module_hashtable *hashtable, long* arg)
{   
    struct process_active   *process;
    struct fiber_struct     *fiber;
    struct fls_struct       *storage;

    unsigned long index;
    unsigned long flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    process = find_process(hashtable, tgid);
    if (!process)
        return -ENOTTY;

    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {
        if (fiber->thread_on == pid)
        {
            storage = &(fiber->fls);
            break;
        }
    }
    if (!fiber || fiber->thread_on != pid)
    {
        spin_unlock_irqrestore(&process->lock, flags);
        return -ENOTTY;
    }
    spin_unlock_irqrestore(&process->lock, flags);

    //spin_lock_irqsave(&storage->fls_lock, flags);

    if (storage->fls == NULL) 
    {
        storage->fls = kvzalloc(sizeof(long long)* MAX_FLS_INDEX, GFP_KERNEL); //we are asking for 32Kb (8 pages)
        storage->size = 0;
        storage->used_index = kzalloc(sizeof(unsigned long)*BITS_TO_LONGS(MAX_FLS_INDEX), GFP_KERNEL); //1 page
    }

    index = find_first_zero_bit(storage->used_index, MAX_FLS_INDEX);   

    if (index < MAX_FLS_INDEX && storage->size < MAX_FLS_INDEX-1) 
    {
        storage->size += 1;
        set_bit(index, storage->used_index);

        if (copy_to_user((void *) arg, (void *) &index, sizeof(long)))
        {
            storage->size -= 1;
            clear_bit(index, storage->used_index);
            //spin_unlock_irqrestore(&storage->fls_lock, flags);
            return -EFAULT;
        } 
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return 0;
        
    }    
    else
    {
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return -ENOTTY;
    }
}

long _ioctl_free(struct module_hashtable *hashtable, long* arg) 
{
    struct process_active   *process;
    struct fls_struct       *storage;
    struct fiber_struct     *fiber;

    unsigned long           index;
    unsigned long           flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);
    
    process = find_process(hashtable, tgid);
    
    if (!process)
        return -ENOTTY;

    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {
        if (fiber->thread_on == pid)
        {
            storage = &(fiber->fls);
            break;
        }
    }
    if (!fiber || fiber->thread_on != pid)
    {
        spin_unlock_irqrestore(&process->lock, flags);
        return -ENOTTY;
    }
    spin_unlock_irqrestore(&process->lock, flags);

    if (copy_from_user((void *) &index, (void *) arg, sizeof(long)))
        return -EFAULT;

    if (index > MAX_FLS_INDEX)
        return -ENOTTY;

    //storage = &(process->fls);
    //spin_lock_irqsave(&storage->fls_lock, flags);
    
    if (storage->fls == NULL || storage->used_index == NULL)
    {
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return -ENOTTY;
    }
    storage->size -= 1;
    clear_bit(index, storage->used_index);
    
    //spin_unlock_irqrestore(&storage->fls_lock, flags);
    
    return 0; 
       
}

long _ioctl_get(struct module_hashtable *hashtable, struct fls_args* args) 
{
    struct process_active   *process;
    struct fls_struct       *storage;
    struct fiber_struct     *fiber;
    
    struct fls_args           ret;

    unsigned long flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    process = find_process(hashtable, tgid);
    
    if (!process)
        return -ENOTTY;

    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {
        if (fiber->thread_on == pid)
        {
            storage = &(fiber->fls);
            break;
        }
    }
    if (!fiber || fiber->thread_on != pid)
    {
        spin_unlock_irqrestore(&process->lock, flags);
        return -ENOTTY;
    }
    spin_unlock_irqrestore(&process->lock, flags);

    if (copy_from_user((void *) &ret, (void *) args, sizeof(struct fls_args)))
        return -EFAULT;

    if ((ret.index) > MAX_FLS_INDEX)
        return -ENOTTY;

    //storage = &(process->fls);
    //spin_lock_irqsave(&storage->fls_lock, flags);
    
    if (test_bit(ret.index, storage->used_index))
    {
        ret.value = storage->fls[ret.index];
        if (copy_to_user((void *) args, (void *) &ret, sizeof(struct fls_args)))
        {   
            //spin_unlock_irqrestore(&storage->fls_lock, flags);
            return -EFAULT;
        }
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return 0;
    }
    else
    {   
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return -ENOTTY;
    }
}

long _ioctl_set(struct module_hashtable *hashtable, struct fls_args* args) 
{
    struct process_active   *process;
    struct fiber_struct     *fiber;
    struct fls_struct       *storage;
    struct fls_args         ret;

    unsigned long flags;

    pid_t tgid;
    pid_t pid;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);
        
    process = find_process(hashtable, tgid);
    
    if (!process) 
        return -ENOTTY;

    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {
        if (fiber->thread_on == pid)
        {
            storage = &(fiber->fls);
            break;
        }
    }
    if (!fiber || fiber->thread_on != pid)
    {
        spin_unlock_irqrestore(&process->lock, flags);
        return -ENOTTY;
    }
    spin_unlock_irqrestore(&process->lock, flags);

    if (copy_from_user((void *) &ret, (void *) args, sizeof(struct fls_args)))
        return -EFAULT;

    if ((ret.index) > MAX_FLS_INDEX)
        return -ENOTTY;

    //storage = &(process->fls);
    //spin_lock_irqsave(&storage->fls_lock, flags);

    if (test_bit(ret.index, storage->used_index))
    {   
        storage->fls[ret.index] = ret.value;
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return 0;
    }
    else
    {   
        //spin_unlock_irqrestore(&storage->fls_lock, flags);
        return -ENOTTY;
    }
}

int _cleanup(struct module_hashtable *hashtable) {
    struct process_active   *process;
    struct fiber_struct     *fiber;
    struct hlist_node       *n;

    pid_t tgid; 
    pid_t pid;

    unsigned long flags;

    tgid = task_tgid_nr(current);
    pid = task_pid_nr(current);

    process = find_process(hashtable, tgid);
    if (!process)
        goto exit_cleanup;

    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry_safe(fiber, n, &process->running_fibers, next) {
        if (fiber->thread_on == pid) {
            hlist_del_rcu(&fiber->next);
            break;
        }
    }
    if (fiber->thread_on != pid)
        fiber = NULL;
    spin_unlock_irqrestore(&process->lock, flags);
    if (fiber) {
        synchronize_rcu();
        kfree(fiber->fls.used_index);
        kvfree(fiber->fls.fls);
        kfree(fiber);
        fiber = NULL;
    }

    //check if it was the last one;
    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry_safe(fiber, n, &process->running_fibers, next)
        break;
    if (fiber) {
        spin_unlock_irqrestore(&process->lock, flags);
        goto exit_cleanup;
    }
    spin_unlock_irqrestore(&process->lock, flags);

    //fiber is null, free everything
    spin_lock_irqsave(&process->lock, flags);
    hlist_for_each_entry_safe(fiber, n, &process->waiting_fibers, next) {
        hlist_del_rcu(&fiber->next);
        synchronize_rcu();    
        kfree(fiber->fls.used_index);
        kvfree(fiber->fls.fls);
        kfree(fiber);
    }
    spin_unlock_irqrestore(&process->lock, flags);

    spin_lock_irqsave(&hashtable->lock, flags);
    hlist_del_rcu(&process->next);
    spin_unlock_irqrestore(&hashtable->lock, flags);
    
    synchronize_rcu();

    kfree(process);
exit_cleanup:
    return 0;
}