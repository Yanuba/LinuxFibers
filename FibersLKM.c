#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <linux/hashtable.h>
#include <linux/slab.h>

#include <linux/kprobes.h>

#include "Fibers_ioctls.h"
#include "Fibers_kioctls.h"

struct module_hashtable process_table;

static int fibers_open(struct inode*, struct file*);
static int fibers_release(struct inode*, struct file*);
static long fibers_ioctl(struct file *, unsigned int, unsigned long);

int kprobe_entry_handler(struct kprobe *, struct pt_regs *);

static int dev_major;
static struct class *device_class = NULL;
static struct device *device = NULL;

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = fibers_open,
    .release = fibers_release,
    .unlocked_ioctl = fibers_ioctl,
    .compat_ioctl = fibers_ioctl,
    };

static struct kprobe probe = {
    .pre_handler = kprobe_entry_handler,
    .symbol_name = "do_exit"
};

static int fibers_open(struct inode* i_node, struct file* filp) {
    struct process_active *process;
    
    unsigned long flags;
    pid_t tgid;

    tgid = task_tgid_nr(current);

    spin_lock_irqsave(&process_table.lock, flags);
    hash_for_each_possible(process_table.htable, process, next, tgid)
    {
        if (process->tgid == tgid)
            break;
    }
    if (process && process->tgid == tgid)
    {
        filp->private_data = process;
        spin_unlock_irqrestore(&process_table.lock, flags);
        return 0;
    }

    process = (struct process_active *) kmalloc(sizeof(struct process_active), GFP_KERNEL);
    if (!process)
    {
        spin_unlock_irqrestore(&process_table.lock, flags);
        return -ENOMEM;
    }
    
    process->tgid = tgid;
    process->next_fid = 0;
    INIT_HLIST_HEAD(&process->running_fibers);
    INIT_HLIST_HEAD(&process->waiting_fibers);
    INIT_HLIST_NODE(&process->next);
    spin_lock_init(&(process->lock));

    filp->private_data = process;
    spin_unlock_irqrestore(&process_table.lock, flags);
    return 0;
}

static int fibers_release(struct inode* i_node, struct file* filp) {
    return 0;
}

static long fibers_ioctl(struct file * filp, unsigned int cmd, unsigned long arg)
{   
    struct task_struct *caller;
    caller = current;

    if (_IOC_DIR(cmd) & _IOC_READ) {
        if (!access_ok(VERIFY_WRITE, arg, _IOC_SIZE(cmd)))
            return -EFAULT;        
    }
    else {
        if (!access_ok(VERIFY_READ, arg, _IOC_SIZE(cmd)))
            return -EFAULT;    
    }

    switch(cmd) 
    {        
        case IOCTL_CONVERT:
            return _ioctl_convert(filp->private_data, (fiber_t *) arg);

        case IOCTL_CREATE:
            return _ioctl_create(filp->private_data, (struct fiber_args*) arg);

        case IOCTL_SWITCH:
            return _ioctl_switch(filp->private_data, (fiber_t *) arg);

        case IOCTL_ALLOC:
            return _ioctl_alloc(filp->private_data, (long *) arg);

        case IOCTL_FREE:
            return _ioctl_free(filp->private_data, (long *) arg);
            
        case IOCTL_GET: 
            return _ioctl_get(filp->private_data, (struct fls_args *) arg);

        case IOCTL_SET:
            return _ioctl_set(filp->private_data, (struct fls_args *) arg);

        default:
            return -ENOTTY; //return value is caught and errno set accordingly
    }
}

int kprobe_entry_handler(struct kprobe *p, struct pt_regs *regs) {
    return _cleanup(&process_table);
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
        ret = dev_major;
        goto fail_regchrdev;
    }
    
    //create class for the device
    device_class = class_create(THIS_MODULE, "Fibers");
    if (IS_ERR(device_class)) {
        ret = PTR_ERR(device_class);
        goto fail_classcreate;
    }

    device_class->devnode = fiber_devnode; //to set permission on device file
    
    //create device
    device = device_create(device_class, NULL, MKDEV(dev_major,0), NULL, KBUILD_MODNAME);
    if (IS_ERR(device)) {
        ret = PTR_ERR(device);
        goto fail_devcreate;
    }

    printk(KERN_NOTICE "%s: Module mounted, device registered with major %d \n", KBUILD_MODNAME, dev_major);

    hash_init(process_table.htable); //should be destroyed?
    spin_lock_init(&process_table.lock);

    ret = register_kprobe(&probe);
    if (ret)
        goto fail_devcreate; //fail chain

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
    unregister_kprobe(&probe);
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
