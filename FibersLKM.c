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

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{   
    struct task_struct *caller;
    caller = current;

    if (_IOC_DIR(cmd) & _IOC_READ) {
        if (!access_ok(VERIFY_WRITE, arg, _IOC_SIZE(cmd)))
        {
            printk_msg("Cannot return data to userspace");
            return -EFAULT;        
        }
    }
    else {
        if (!access_ok(VERIFY_READ, arg, _IOC_SIZE(cmd)))
        {
            printk_msg("Cannot read data from userspace");
            return -EFAULT;    
        }
    }

    switch(cmd) 
    {        
        case IOCTL_CONVERT:
            return _ioctl_convert(&process_table, (fiber_t *) arg);

        case IOCTL_CREATE:
            return _ioctl_create(&process_table, (struct fiber_args*) arg);

        case IOCTL_SWITCH:
            return _ioctl_switch(&process_table, (fiber_t *) arg);

        case IOCTL_ALLOC:
            return _ioctl_alloc(&process_table, (long *) arg);

        case IOCTL_FREE:
            return _ioctl_free(&process_table, (long *) arg);
            
        case IOCTL_GET: 
            return _ioctl_get(&process_table, (struct fls_args *) arg);

        case IOCTL_SET:
            return _ioctl_set(&process_table, (struct fls_args *) arg);

        default:
            return -ENOTTY; //return value is caught and errno set accordingly
    }
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

    hash_init(process_table.htable); //should be destroyed?
    spin_lock_init(&process_table.lock);

    return 0;

    fail_devcreate:
        class_unregister(device_class);
        class_destroy(device_class);
    fail_classcreate:
        unregister_chrdev(dev_major, KBUILD_MODNAME);
    fail_regchrdev:
        return ret;
}

/* Missing clean_up of data structures */
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
MODULE_AUTHOR("Andrea Mastropietro <mastropietro.1652886@studenti.uniroma1.it>");
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);
