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

struct fiber_struct {
    pid_t fiber_id;
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
 */

/* 
 * Hash table, each bucket should refer to a process, we anyway check the pid in case of conflicts (but this barely happens)
 * Max pid is 32768, which require 16 bytes (so 15 bits are enough for not having collisions)
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

    struct process_active *process_cursor;
    struct fiber_struct *fiber_cursor;

    //maybe is not needed
    struct fiber_struct_usr *idx_next; 

    caller_pid = task_tgid_nr(current);
    caller_tid = task_pid_nr(current);

    switch(cmd) {        
        case IOCTL_CONVERT:
            /* 
             * @TO_DO
             * Check whether the current thread is already a fiber
             * Allocate a new Fiber
             * Return data to userspace
             */
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d of process %d\n", KBUILD_MODNAME, caller_tid, caller_pid);
            
            //can we return data to userspace?
            if (!access_ok(VERIFY_WRITE, arg, sizeof(fiber_id))) {   
                printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return data to userspace\n", KBUILD_MODNAME);
            }

            hash_for_each_possible(process_table, process_cursor, other, caller_pid) {
                if (process_cursor->pid == caller_pid) {
                    printk(KERN_NOTICE "%s: Process %d is activated\n", KBUILD_MODNAME, caller_pid);
                    //look for fiber (only active one are needed)
                        //if fiber for current thread already exist return error
                        //else allocate a new fiber context
                }
            }

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

            //allocate new fiber context
            fiber_cursor = (struct fiber_struct *) kzalloc(sizeof(struct fiber_struct), GFP_KERNEL);
            fiber_cursor->fiber_id = process_cursor->num_fiber++;
            fiber_cursor->parent_process = process_cursor->pid;
            fiber_cursor->parent_thread = caller_tid;
            fiber_cursor->status = FIBER_RUNNING;
            /* 
             * We must save pt_regs and fpu_regs now? Maybe yes for saving the entrypoint (which is the entrypoint in this case?)
             * Init other Fiber fields: 
             * activations,
             * failed activations,
             * execution time;
             * */
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d: new fiber context allocated, id is %d\n", KBUILD_MODNAME, caller_tid, fiber_cursor->fiber_id);


            //add new fiber
            hlist_add_head(&fiber_cursor->next, &process_cursor->running_fibers);

            //return pid of newly created fiber.
            if (copy_to_user((void *) arg, (void *) &fiber_cursor->fiber_id, sizeof(fiber_id))) {
                //cannot copy, must do something
                printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return fiber id\n", KBUILD_MODNAME);    
            }
            

            return 0;
            /*
            ConvertThreadToFiber(): creates a Fiber in the current thread. 
            From now on, other Fibers can be created.
            */       
            break;

        case IOCTL_CREATE:
            
            /*
            CreateFiber(): creates a new Fiber context,
            assigns a separate stack, sets up the execution entry
            point (associated to a function passed as argument to
            the function)Fibers
            */
            printk(KERN_NOTICE "%s: 'CreateFiber()' Not Implemented yet\n", KBUILD_MODNAME);
            break;

        case IOCTL_SWITCH:

            printk(KERN_NOTICE "%s: 'SwitchToFiber()' Does nothing yet\n", KBUILD_MODNAME);
            idx_next = kmalloc(sizeof(struct fiber_struct_usr),GFP_KERNEL);
            if (copy_from_user(idx_next, (void *) arg, sizeof(struct fiber_struct_usr))) { //help, if idx_next is deleted how we can do this?
                //cannot copy, must do something
                printk(KERN_NOTICE "%s: Cannot take input from userspace\n", KBUILD_MODNAME);    
            }
            printk(KERN_NOTICE "%s: Copyed input from userspace, %d\n", KBUILD_MODNAME, idx_next->info);   
            kfree(idx_next);
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
            return -ENOTTY;
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
