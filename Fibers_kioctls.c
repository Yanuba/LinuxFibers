#include <linux/slab.h>

//for pt_regs
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
//for fpu
#include <asm/fpu/internal.h>

#include "Fibers_kioctls.h"

struct fiber_struct* allocate_fiber(pid_t fiber_id, struct task_struct *p, void* (entry_point)(void*), void* args, void* stack_base) {
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
        fpu__save(&(fiber->fpu_regs));

        if (entry_point != NULL) {
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

long _ioctl_convert(struct module_hashtable *hashtable, fiber_t* arg, struct task_struct* p){    
    struct process_active   *process;
    struct fiber_struct     *fiber;
    struct hlist_node       *cursor;
    pid_t tgid; 
    pid_t pid;
    
    tgid = task_tgid_nr(p);
    pid = task_pid_nr(p);

    hash_for_each_possible(hashtable->htable, process, next, tgid) {
        if (process->tgid == tgid) { //Needed in case of collision
            printk(KERN_NOTICE "%s:ConvertThreadToFiber() Process %d is activated\n", KBUILD_MODNAME, tgid);
            hlist_for_each(cursor, &process->running_fibers) {
                fiber = hlist_entry(cursor, struct fiber_struct, next);
                if (fiber->parent_thread == tgid) {
                    printk(KERN_NOTICE "%s:ConvertThreadToFiber() Thread %d is alrady a fiber\n", KBUILD_MODNAME, tgid);
                    return -ENOTTY; //
                }
            }
            goto ALLOCATE_FIBER;
        }
    }

    printk(KERN_NOTICE "%s:ConvertThreadToFiber() Thread %d No active process found\n", KBUILD_MODNAME, pid); 
    //create new struct process information
    process = (struct process_active *) kmalloc(sizeof(struct process_active), GFP_KERNEL);
    process->tgid = tgid;
    process->next_fid = 1;
    INIT_HLIST_HEAD(&process->running_fibers);
    INIT_HLIST_HEAD(&process->waiting_fibers);
    INIT_HLIST_NODE(&process->next);
    hash_add(hashtable->htable, &process->next, tgid);
    printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: new process info allocated\n", KBUILD_MODNAME, pid);

ALLOCATE_FIBER:
    fiber = allocate_fiber(process->next_fid, p, NULL, NULL, NULL);
    hlist_add_head(&(fiber->next), &(process->running_fibers));
  
    if (copy_to_user((void *) arg, (void *) &fiber->fiber_id,sizeof(fiber_t))) {
        printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return fiber id\n", KBUILD_MODNAME);
        hlist_del(&fiber->next);
        kfree(fiber);
        //Process info can stay
        return -EFAULT;
    }
 
    printk(KERN_NOTICE "%s:Thread %d, ConvertThreadToFiber() was succesful, exiting...\n", KBUILD_MODNAME, pid); 
    return 0;
}