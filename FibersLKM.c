#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include <linux/hashtable.h>

//for pt regs and fpu
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
#include <asm/fpu/internal.h>

#include "FibersLKM.h"
#include "Fibers.h"

struct fiber_struct {
    pid_t fiber_id;
    pid_t thread_on;        //last thread the fiber runs on
    struct hlist_node next; //must queue fibers
    
    //maybe we need only the pointer?
    struct pt_regs regs; //regs = task_pt_regs(current); (where regs is of type struct pt_regs*)  
    struct fpu fpu_regs; //fpu__save(struct fpu *) and fpu__restore(struct fpu *)?

    /* Fields to be exposed in proc */
    short   status;                 //The fiber is running or not?
    void*   (*entry_point)(void*);  //entry point of the fiber (EIP)
    pid_t   parent_process;         //key in the hash table
    pid_t   parent_thread;          //pid of the thread that created the fiber
    int     activations;            //the number of current activations of the Fiber
    int     failed_activations;     //the number of failed activations
    long    execution_time;         //total execution time in that Fiber context
};

/*
 * Only a fiber can switch to another fiber. 
 * If we want to use switchTo, we must use ConvertThreadTofiber in the current thread.
 * Fibers are shared across the process, a Fiber can switch to a fiber created in another thread.
 * 
 * We can keep a list of threads relying on fibers, and a list of active fibers within a process. (See notes for details.)
 * 
 * How to keep the info about the fiber running in the current thread?
 * 
 * HOW TO CLEANUP? Probe do_exit?
 *
 * we have to initialize hlist_node?
 * 
 */

/* 
 * Hash table, each bucket should refer to a process, we anyway check the pid in case of conflicts (but this barely happens)
 * Max pid on this system is 32768, if we exclude pid 0 we can have 32767 different pids,
 * which require 15 bits (which are crearly enough for not having collisions)
 * */
DEFINE_HASHTABLE(process_table, 15);

struct process_active {
    pid_t pid;                          //process id
    pid_t num_fiber;                    //will be next fiber id                        
    struct hlist_head running_fibers;   //running fibers of the process
    struct hlist_head waiting_fibers;   //waiting fibers of the process
    struct hlist_node other;            //other process in the bucket
};

/* ******************************************************************* */

static long fibers_ioctl(struct file *, unsigned int, unsigned long);

static int dev_major;
static struct class *device_class = NULL;
static struct device *device = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fibers_ioctl,
    .compat_ioctl = fibers_ioctl, //for 32 bit on 64, works?
};

/* ******************************************************************* */

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{   
    pid_t caller_pid;
    pid_t caller_tid;

    struct hlist_node* hlist_cursor;
    struct process_active *process_cursor;
    struct fiber_struct *fiber_cursor;

    //maybe is not needed
    struct s_create_args *usr_args; 
    struct pt_regs * reg_cur;

    caller_pid = task_tgid_nr(current);
    caller_tid = task_pid_nr(current);

    switch(cmd) {        
        case IOCTL_CONVERT:
            /* 
             * @TO_DO
             * Allocate a new Fiber
             * Return data to userspace
             * Cleanup in case of error
             * Proc exposure
             */
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d of process %d\n", KBUILD_MODNAME, caller_tid, caller_pid);
            
            //can we return data to userspace?
            if (!access_ok(VERIFY_WRITE, arg, sizeof(fiber_id))) {
                printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            hash_for_each_possible(process_table, process_cursor, other, caller_pid) {
                if (process_cursor->pid == caller_pid) {
                    printk(KERN_NOTICE "%s: Process %d is activated\n", KBUILD_MODNAME, caller_pid);

                    hlist_for_each(hlist_cursor, &process_cursor->running_fibers) {
                        fiber_cursor = hlist_entry(hlist_cursor, struct fiber_struct, next);
                        if (fiber_cursor->parent_thread == caller_tid) {
                            printk(KERN_NOTICE "%s: Thread %d is already a fiber\n", KBUILD_MODNAME, caller_tid);
                            return -EEXIST; //Thread is already a fiber
                        }                
                    }
                    goto allocate_fiber;
                }
            }

            printk(KERN_NOTICE "%s:  ConvertThreadToFiber() No active process found\n", KBUILD_MODNAME); 

            //create new struct process information
            process_cursor = (struct process_active *) kmalloc(sizeof(struct process_active), GFP_KERNEL);
            process_cursor->pid = caller_pid;
            process_cursor->num_fiber = 1;       
            INIT_HLIST_HEAD(&process_cursor->running_fibers);
            INIT_HLIST_HEAD(&process_cursor->waiting_fibers);
            INIT_HLIST_NODE(&process_cursor->other);
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: new process info allocated\n", KBUILD_MODNAME, caller_tid);

            //add process information to hash table
            hash_add(process_table, &process_cursor->other, process_cursor->pid);
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: new process info added to table\n", KBUILD_MODNAME, caller_tid);

        allocate_fiber:
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: allocating new fiber\n", KBUILD_MODNAME, caller_tid);
            //allocate new fiber context
            fiber_cursor = (struct fiber_struct *) kzalloc(sizeof(struct fiber_struct), GFP_KERNEL);
            fiber_cursor->fiber_id = process_cursor->num_fiber++;
            fiber_cursor->parent_process = process_cursor->pid;
            fiber_cursor->parent_thread = fiber_cursor->thread_on = caller_tid;
            fiber_cursor->activations = 1;
            fiber_cursor->failed_activations = 0;
            fiber_cursor->status = FIBER_RUNNING;
            INIT_HLIST_NODE(&fiber_cursor->next);
            /* 
             * We must save pt_regs and fpu_regs now? Maybe yes for saving the entrypoint (which is the entrypoint in this case?)
             * Init other Fiber fields: 
             * execution time;
             * */
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: new fiber context allocated, id is %d\n", KBUILD_MODNAME, caller_tid, fiber_cursor->fiber_id);

            //add new fiber
            hlist_add_head(&fiber_cursor->next, &process_cursor->running_fibers);

            //return pid of newly created fiber.
            if (copy_to_user((void *) arg, (void *) &fiber_cursor->fiber_id, sizeof(fiber_id))) {
                printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return fiber id\n", KBUILD_MODNAME);
                //cannot copy, must do something?
                //cleanup and return error 
                return -ENOTTY; //?
            }
            
            printk(KERN_NOTICE "%s:  ConvertThreadToFiber() was succesful, exiting...\n", KBUILD_MODNAME); 
            return 0;      

        case IOCTL_CREATE:

            if (!access_ok(VERIFY_WRITE, arg, sizeof(struct s_create_args))) {
                printk(KERN_NOTICE "%s:  CreateFiber() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d of process %d\n", KBUILD_MODNAME, caller_tid, caller_pid);
            
            hash_for_each_possible(process_table, process_cursor, other, caller_pid) {
                if (process_cursor->pid == caller_pid) {
                    
                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, process found\n", KBUILD_MODNAME, caller_tid);
                    usr_args = (struct s_create_args *) kmalloc(sizeof(struct s_create_args), GFP_KERNEL);

                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Copying args\n", KBUILD_MODNAME, caller_tid);
                    if (copy_from_user((void *) usr_args, (void *) arg, sizeof(struct s_create_args))) {
                        printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Cannot copy args\n", KBUILD_MODNAME, caller_tid);
                        kfree(usr_args);
                        return -ENOTTY; //?
                    }

                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Allocating Fiber\n", KBUILD_MODNAME, caller_tid);
                    fiber_cursor = (struct fiber_struct *) kmalloc(sizeof(struct fiber_struct), GFP_KERNEL);
                    fiber_cursor->fiber_id = usr_args->ret = process_cursor->num_fiber++;
                    fiber_cursor->parent_process = caller_pid;
                    fiber_cursor->parent_thread = fiber_cursor->thread_on = caller_tid;
                    fiber_cursor->status = FIBER_WAITING;
                    fiber_cursor->activations = fiber_cursor->failed_activations = 0;
                    fiber_cursor->entry_point = usr_args->routine;
                    fiber_cursor->execution_time = 0;
                    INIT_HLIST_NODE(&fiber_cursor->next);

                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Setting up initial state\n", KBUILD_MODNAME, caller_tid);
                    //registers status (maybe cannot do it in this way)
                    reg_cur = &(fiber_cursor->regs);
                    reg_cur->ip = (unsigned long) usr_args->routine;
                    reg_cur->di = (unsigned long) usr_args->routine_args;
                    reg_cur->sp = reg_cur->bp = (unsigned long) usr_args->stack_address; //end of stack?
                    //populate fiber struct
                    //thread on and fpu are don't care in this state.    

                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Adding fiber to hashtable\n", KBUILD_MODNAME, caller_tid);
                    hlist_add_head(&fiber_cursor->next, &process_cursor->waiting_fibers);

                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Returning to user fiber_if %d\n", KBUILD_MODNAME, caller_tid, fiber_cursor->fiber_id);
                    if (copy_to_user((void *) arg, (void *) usr_args, sizeof(struct s_create_args))) {
                        /* Something went wrong, 
                         * Cannot return data to userspace
                         * Cleanup and return error
                         * */
                        return -ENOTTY; //?
                    }
                    
                    kfree(usr_args);//we don't need this anymore
                    
                    printk(KERN_NOTICE "%s: CreateFiber() called by thread %d, Exiting succesfully\n", KBUILD_MODNAME, caller_tid);
                    return 0;
                }
            }
            
            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d cannot be executed sice it is not a Fiber\n", KBUILD_MODNAME, caller_tid);
            return -ENOTTY;

            /*
             *CreateFiber(): creates a new Fiber context,
             *assigns a separate stack, sets up the execution entry
             *point (associated to a function passed as argument to
             *the function)
             * */
            
            break;

        case IOCTL_SWITCH:

            /*
            printk(KERN_NOTICE "%s: 'SwitchToFiber()' Does nothing yet\n", KBUILD_MODNAME);
            idx_next = kmalloc(sizeof(struct fiber_struct_usr),GFP_KERNEL);
            if (copy_from_user(idx_next, (void *) arg, sizeof(struct fiber_struct_usr))) { //help, if idx_next is deleted how we can do this?
                //cannot copy, must do something
                printk(KERN_NOTICE "%s: Cannot take input from userspace\n", KBUILD_MODNAME);    
            }
            printk(KERN_NOTICE "%s: Copyed input from userspace, %d\n", KBUILD_MODNAME, idx_next->info);   
            kfree(idx_next);
            */
            //look for the fiber in the module memory
            //  If (found):
            //      Do the checks and eventually the context switch;
            //  else:
            //      Do nothing
            
            /*
            SwitchToFiber(): switches the execution context
            (in the caller thread) to a different Fiber (it can fail if
            switching to a Fiber which is already active)
            */   
            break;

        case IOCTL_ALLOC:
            
            /*
            FlsAlloc() : Allocates a FLS index
            */ 
            printk(KERN_NOTICE "%s: 'FlsAlloc()' Not Implemented yet\n", KBUILD_MODNAME);
            break;

        case IOCTL_FREE: 
            
            /*
            FlsFree() : Frees a FLS index
            */
            printk(KERN_NOTICE "%s: 'FlsFree()' Not Implemented yet\n", KBUILD_MODNAME);
            break;

        case IOCTL_GET: 
            
            /*
            FlsGetValue() : Gets the value associated with a FLS
            index (a long)
            */
            printk(KERN_NOTICE "%s: 'FlsGetValue()' Not Implemented yet\n", KBUILD_MODNAME);
            break;

        case IOCTL_SET:
            
            /*
            FlsSetValue() : Sets a value associated with a FLS
            index (a long)
            */
            printk(KERN_NOTICE "%s: 'FlsSetValue()' Not Implemented yet\n", KBUILD_MODNAME);
            break;

        default:
            printk(KERN_NOTICE "%s: Default ioctl called\n", KBUILD_MODNAME);
            return -ENOTTY; //return value is caught and errno set accordingly
            break;
    }
    return 0;
}

//Copyed from tty driver
static char *fiber_devnode(struct device *dev, umode_t *mode)
{
    if (!mode)
            return NULL;
    if (dev->devt == MKDEV(dev_major, 0) ||
        dev->devt == MKDEV(dev_major, 2))
            *mode = 0666;
    return NULL;
}

static int __init fibers_init(void) 
{   
    int ret;

    /* Device allocation code */

    //Allocate a major number
    dev_major = register_chrdev(0, KBUILD_MODNAME, &fops);
    if (dev_major < 0) {
        printk(KERN_ERR "%s: Failed registering char device\n", KBUILD_MODNAME);
        ret = dev_major;
        goto fail_regchrdev;
    }
    
    //create class for the device
    device_class = class_create(THIS_MODULE, "Fibers");
    if (IS_ERR(device_class)) {
        printk(KERN_ERR "%s: Failed to create device class\n", KBUILD_MODNAME);
        ret = PTR_ERR(device_class);
        goto fail_classcreate;
    }

    device_class->devnode = fiber_devnode; //to set permission on device file
    
    //create device
    device = device_create(device_class, NULL, MKDEV(dev_major,0), NULL, KBUILD_MODNAME);
    if (IS_ERR(device)) {
        printk(KERN_ERR "%s: Failed to create device class\n", KBUILD_MODNAME);
        ret = PTR_ERR(device);
        goto fail_devcreate;
    }

    printk(KERN_NOTICE "%s: Module mounted, device registered with major %d \n", KBUILD_MODNAME, dev_major);

    hash_init(process_table); 

    return 0;

    fail_devcreate:
        class_unregister(device_class);
        class_destroy(device_class);
    fail_classcreate:
        unregister_chrdev(dev_major, KBUILD_MODNAME);
    fail_regchrdev:
        return ret;
}

static void __exit fibers_exit(void)
{   
    device_destroy(device_class, MKDEV(dev_major,0));
    class_unregister(device_class);
    class_destroy(device_class);
    unregister_chrdev(dev_major, KBUILD_MODNAME);
    
    printk(KERN_NOTICE "%s: Module un-mounted\n", KBUILD_MODNAME);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Umberto Mazziotta <mazziotta.1647818@studenti.uniroma1.it>");
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);
