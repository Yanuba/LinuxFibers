#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h> //for file system operations
#include <linux/device.h>
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h> //this makes me feel really bad :(

#include "FibersLKM.h"


static long fibers_ioctl(struct file *, unsigned int, unsigned long);

static int dev_major;
static struct class *device_class = NULL;
static struct device *device = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fibers_ioctl,
    .compat_ioctl = fibers_ioctl, //for 32 bit on 64, works?
};

/* Here we store only the status of one 'Fiber' */
struct pt_regs *regs = NULL;
/* For context switch thry using thread_struct or tss_struct
 * Download Quaglia slides, it may conta9in useful info for this project
 */

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd) {
        case IOCTL_EX:
            printk(KERN_NOTICE "%s: Pid is %d\n", KBUILD_MODNAME, current->pid);
            
            /* WIll this work? */
            regs = task_pt_regs(current);   
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
            break;
        case IOCTL_CONVERT:
            /*
            ConvertThreadToFiber(): creates a Fiber in the current thread. 
            From now on, other Fibers can be created.
            */       
            printk(KERN_NOTICE "%s: 'ConvertThreadToFiber()' Not Implemented yet\n", KBUILD_MODNAME);
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
            printk(KERN_NOTICE "%s: 'SwitchToFiber()' Not Implemented yet\n", KBUILD_MODNAME);
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
