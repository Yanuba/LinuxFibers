#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h> //for file system operations

#include "FibersLKM.h"


static long fibers_ioctl(struct file *, unsigned int, unsigned long);

static int dev_major;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = fibers_ioctl,
    .compat_ioctl = fibers_ioctl, //for 32 bit on 64, works?
};

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{
    switch(cmd) {
        case 0:
            printk(KERN_NOTICE "%s: Default ioctl called\n", KBUILD_MODNAME);
            break;
        default:
            printk(KERN_NOTICE "%s: Default ioctl called\n", KBUILD_MODNAME);
    }

    return 0;
}

static int __init fibers_init(void) 
{   
    int ret;

    dev_major = register_chrdev(0, KBUILD_MODNAME, &fops);
    if (dev_major < 0) {
        printk(KERN_ERR "%s: Failed registering char device\n", KBUILD_MODNAME);
        ret = dev_major;
        goto fail_regchrdev;
    }
    printk(KERN_NOTICE "%s: Module mounted\n", KBUILD_MODNAME);
    return 0;

    fail_regchrdev:
        return ret;
}

static void __exit fibers_exit(void)
{
    unregister_chrdev(dev_major, KBUILD_MODNAME);
    printk(KERN_NOTICE "%s: Module un-mounted\n", KBUILD_MODNAME);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Umberto Mazziotta <mazziotta.1647818@studenti.uniroma1.it>");
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);