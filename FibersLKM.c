#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/slab.h>


/*
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/fs.h> //for file system operations
*/


#include "FibersLKM.h"

struct fiber_struct {
    /* How we save the context? pt_regs? To perform contex switch it may be useful use thread_struct or tss_struct */

    struct list_head queue; //to queue fibers

    /* Fields to be exposed in proc */
    short   status;                 //The fiber is running or not?
    void*   (*entry_point)(void*);   //entry point of the fiber
    pid_t   parent_thread;          //pid of the thread that created the fiber
    int     activations;            //the number of current activations of the Fiber
    int     failed_activations;     //the number of failed activations
    long    execution_time;         //total execution time in that Fiber context
};


/*
 * Need to increment module reference counter, to avoid to unload the module when some thread is using it 
 * */

/*
 * Only a fiber can switch to another fiber. 
 * If we want to use switchTo, we must use ConvertThreadTofiber in the current thread.
 * Fibers are shared across the process, a Fiber can switch to a fiber created in another thread.
 * 
 * We can keep a list of threads relying on fibers, and a list of active fibers within a process. (See notes for details.)
 * 
 * How to keep the info about the fiber running in the current thread?
 * 
 */
struct active_process {
    pid_t pid;                          //process id
    struct list_head other;             //other process in the list
    struct list_head running_fibers;    //running fibers of the process
    struct list_head waiting_fibers;    //waiting fibers of the process
};

//struct active_process* pocess_list = NULL;
struct list_head process_list;

/*
 * We may keep for each process two lists: a list of running fibers, and a list of not running ones.
 * */

static long fibers_ioctl(struct file *, unsigned int, unsigned long);

static int dev_major;
static struct class *device_class = NULL;
static struct device *device = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fibers_ioctl,
    .compat_ioctl = fibers_ioctl, //for 32 bit on 64, works?
};

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{   
    pid_t caller_pid;
    pid_t caller_tid;

    //maybe is not needed
    struct fiber_struct_usr *idx_next; 
    struct active_process *process;
    struct fiber_struct *fiber;
    struct list_head *cursor;

    caller_pid = task_tgid_nr(current);
    caller_tid = task_pid_nr(current);

    printk(KERN_NOTICE "%s: The caller process is:%d, the thread id %d\n", KBUILD_MODNAME, caller_pid, caller_tid);

    switch(cmd) { 
        
        /* Need some adjustement */        
        case IOCTL_CONVERT:
            printk(KERN_NOTICE "%s: ConvertThreadToFiber()\n", KBUILD_MODNAME);
            list_for_each(cursor, &process_list) {
                process = list_entry(cursor, struct active_process, other);
                if (process->pid == caller_pid) {
                    printk(KERN_NOTICE "%s: Process %d is activate\n", KBUILD_MODNAME, caller_pid);
                    goto look_for_fiber;
                }
            }
            
            //we have found nothing, create a new active process
            process = (struct active_process *) kmalloc(sizeof(struct active_process), GFP_KERNEL);
            process->pid = caller_pid;
            
            INIT_LIST_HEAD(&(process->other));
            INIT_LIST_HEAD(&(process->running_fibers));
            INIT_LIST_HEAD(&(process->waiting_fibers));
            
            list_add_tail(&(process->other), &process_list);
            printk(KERN_NOTICE "%s: Process %d is now active\n", KBUILD_MODNAME, caller_pid);

look_for_fiber:
            printk(KERN_NOTICE "%s: Looking for the fiber\n", KBUILD_MODNAME);
            list_for_each(cursor, &(process->running_fibers)) {
                fiber = list_entry(cursor, struct fiber_struct, queue);
                if (fiber->parent_thread == caller_tid) {
                    printk(KERN_NOTICE "%s: Fiber for thread %d are active\n", KBUILD_MODNAME, caller_tid);
                    return -1; //change this
                }
            }
            list_for_each(cursor, &(process->waiting_fibers)) {
                fiber = list_entry(cursor, struct fiber_struct, queue);
                if (fiber->parent_thread == caller_tid) {
                    printk(KERN_NOTICE "%s: Fiber for thread %d are active\n", KBUILD_MODNAME, caller_tid);
                    return -1; //change this
                }
            }
        
        printk(KERN_NOTICE "%s: Allocating first fiber for thread %d\n", KBUILD_MODNAME, caller_tid);

            fiber = (struct fiber_struct *) kmalloc(sizeof(struct fiber_struct), GFP_KERNEL);
            /*
             * Initilize fiber data
             * */
            fiber->status = FIBER_WAITING;
            fiber->parent_thread = caller_tid;
            //other data initialization

            list_add_tail(&(fiber->queue), &(process->waiting_fibers));

            /*
             * Initilize fiber data to pass userspace
             * */
            idx_next = (struct fiber_struct_usr *) kmalloc(sizeof(struct fiber_struct_usr), GFP_KERNEL);

            if (copy_to_user((void *) arg, idx_next, sizeof(struct fiber_struct_usr))) {
                //cannot copy, must do something
                printk(KERN_NOTICE "%s: Cannot pass input to userspace\n", KBUILD_MODNAME);    
            }
            
            printk(KERN_NOTICE "%s: Passed input to userspace\n", KBUILD_MODNAME);
            kfree(idx_next);
            return 0;

            /*
            ConvertThreadToFiber(): creates a Fiber in the current thread. 
            From now on, other Fibers can be created.
            */       
            //break;

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

            printk(KERN_NOTICE "%s: Pid is %d\n", KBUILD_MODNAME, current->pid);
            //regs = task_pt_regs(current);   
            /*
            {          
            printk(KERN_NOTICE "%s: r15 is %ld\n", KBUILD_MODNAME, regs->r15);
            printk(KERN_NOTICE "%s: r14 is %ld\n", KBUILD_MODNAME, regs->r14);
            printk(KERN_NOTICE "%s: r13 is %ld\n", KBUILD_MODNAME, regs->r13);
            printk(KERN_NOTICE "%s: r12 is %ld\n", KBUILD_MODNAME, regs->r12);
            printk(KERN_NOTICE "%s: rbp is %ld\n", KBUILD_MODNAME, regs->bp);
            printk(KERN_NOTICE "%s: rbx is %ld\n", KBUILD_MODNAME, regs->bx);
            printk(KERN_NOTICE "%s: r11 is %ld\n", KBUILD_MODNAME, regs->r11);
            printk(KERN_NOTICE "%s: r10 is %ld\n", KBUILD_MODNAME, regs->r10);
            printk(KERN_NOTICE "%s: r9  is %ld\n", KBUILD_MODNAME, regs->r9);
            printk(KERN_NOTICE "%s: r8  is %ld\n", KBUILD_MODNAME, regs->r8);
            printk(KERN_NOTICE "%s: rax is %ld\n", KBUILD_MODNAME, regs->ax);
            printk(KERN_NOTICE "%s: rcx is %ld\n", KBUILD_MODNAME, regs->cx);
            printk(KERN_NOTICE "%s: rdx is %ld\n", KBUILD_MODNAME, regs->dx);
            printk(KERN_NOTICE "%s: rsi is %ld\n", KBUILD_MODNAME, regs->si);
            printk(KERN_NOTICE "%s: rdi is %ld\n", KBUILD_MODNAME, regs->di);
            printk(KERN_NOTICE "%s: o_rax is %ld\n", KBUILD_MODNAME, regs->orig_ax);
            printk(KERN_NOTICE "%s: rip is %ld\n", KBUILD_MODNAME, regs->ip);
            printk(KERN_NOTICE "%s: cs  is %ld\n", KBUILD_MODNAME, regs->cs);
            printk(KERN_NOTICE "%s: eflags is %ld\n", KBUILD_MODNAME, regs->flags);
            printk(KERN_NOTICE "%s: rsp is %ld\n", KBUILD_MODNAME, regs->sp);
            printk(KERN_NOTICE "%s: ss  is %ld\n", KBUILD_MODNAME, regs->ss);
            printk(KERN_NOTICE "%s: Please don't puke \n", KBUILD_MODNAME);
            }
            */
            
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
    INIT_LIST_HEAD(&process_list);

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
