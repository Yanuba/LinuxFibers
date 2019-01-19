#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <linux/hashtable.h>

#include "Fibers_ktypes.h"
#include "Fibers_ioctls.h"
#include "Fibers_kioctls.h"

struct module_hashtable process_table;

static long fibers_ioctl(struct file *, unsigned int, unsigned long);

static int dev_major;
static struct class *device_class = NULL;
static struct device *device = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fibers_ioctl,
    .compat_ioctl = fibers_ioctl, //for 32 bit on 64, works?
};

//use macros to extrac arg sizes in IOCTL function (They exists for some reason)

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{   
    struct task_struct *caller;
    caller = current;

    switch(cmd) 
    {        
        case IOCTL_CONVERT:
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d of process %d\n", KBUILD_MODNAME, task_pid_nr(caller), task_tgid_nr(caller));
            
            if (!access_ok(VERIFY_WRITE, arg, sizeof(fiber_t)))
            {
                printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            return _ioctl_convert(&process_table, (fiber_t *) arg);

        case IOCTL_CREATE:
            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d of process %d\n", KBUILD_MODNAME, task_pid_nr(caller), task_tgid_nr(caller));

            if (!access_ok(VERIFY_WRITE, arg, sizeof(struct fiber_args)))
            {
                printk(KERN_NOTICE "%s:  CreateFiber() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            return _ioctl_create(&process_table, (struct fiber_args*) arg);

        case IOCTL_SWITCH:
            if (!access_ok(VERIFY_READ, arg, sizeof(fiber_t))) 
            {
                printk(KERN_NOTICE "%s:  SwitchToFiber() cannot read data from userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            return _ioctl_switch(&process_table, (fiber_t *) arg);

        case IOCTL_ALLOC:
            if (!access_ok(VERIFY_WRITE, arg, sizeof(long)))
            {
                printk(KERN_NOTICE "%s: FlsAlloc() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT;
            }

            return _ioctl_alloc(&process_table, (long *) arg);

        case IOCTL_FREE: 
            if (!access_ok(VERIFY_READ, arg, sizeof(long)))
            {
                printk(KERN_NOTICE "%s: FlsFree() cannot read data from userspace\n", KBUILD_MODNAME);
                return -EFAULT;
            }
            
            return _ioctl_free(&process_table, (long *) arg);
            
        case IOCTL_GET: 
            
            if (!access_ok(VERIFY_WRITE, arg, sizeof(struct fls_args)))
            {
                printk(KERN_NOTICE "%s: FlsFree() cannot read data from userspace\n", KBUILD_MODNAME);
                return -EFAULT;
            }
            
            return _ioctl_get(&process_table, (struct fls_args *) arg);

        case IOCTL_SET:
            
            if (!access_ok(VERIFY_READ, arg, sizeof(struct fls_args)))
            {
                printk(KERN_NOTICE "%s: FlsFree() cannot read data from userspace\n", KBUILD_MODNAME);
                return -EFAULT;
            }
            
            return _ioctl_set(&process_table, (struct fls_args *) arg);

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

    hash_init(process_table.htable); 

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
MODULE_AUTHOR("Andrea Mastropietro <mastropietro.1652886@studenti.uniroma1.it>")
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);
