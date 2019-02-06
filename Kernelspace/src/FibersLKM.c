#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/hashtable.h>
#include <linux/slab.h>
#include <linux/kprobes.h>

#include "fibers_ktypes.h"
#include "Fibers_kioctls.h"
#include "proc.h"

static struct module_hashtable process_table;

static int fibers_open(struct inode *, struct file *);
static int fibers_release(struct inode *, struct file *);
static long fibers_ioctl(struct file *, unsigned int, unsigned long);

static int kprobe_cleanup_handler(struct kprobe *, struct pt_regs *);
static int kprobe_proc_lookup_handler(struct kretprobe_instance *, struct pt_regs *);
static int kprobe_proc_readdir_handler(struct kretprobe_instance *, struct pt_regs *);

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
    .pre_handler = kprobe_cleanup_handler,
    .symbol_name = "do_exit"};

static struct kretprobe readdir_probe = {
    .kp = {.symbol_name = "proc_pident_readdir"},
    .handler = kprobe_proc_post_readdir_handler,
    .entry_handler = kprobe_proc_readdir_handler,
    .data_size = sizeof(struct kret_data),
};

static struct kretprobe lookup_probe = {
    .kp = {.symbol_name = "proc_pident_lookup"},
    .handler = kprobe_proc_post_lookup_handler,
    .entry_handler = kprobe_proc_lookup_handler,
    .data_size = sizeof(struct kret_data),
};

// for permissions, no sudo needed
static char *fiber_devnode(struct device *dev, umode_t *mode)
{
    if (!mode)
        return NULL;
    *mode = 0666;
    return NULL;
}

static int fibers_open(struct inode *i_node, struct file *filp)
{
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

    process = (struct process_active *)kmalloc(sizeof(struct process_active), GFP_KERNEL);
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
    hash_add(process_table.htable, &process->next, tgid);

    spin_unlock_irqrestore(&process_table.lock, flags);

    return 0;
}

static int fibers_release(struct inode *i_node, struct file *filp)
{
    return 0;
}

static long fibers_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    if (_IOC_DIR(cmd) & _IOC_READ)
    {
        if (!access_ok(VERIFY_WRITE, arg, _IOC_SIZE(cmd)))
            return -EFAULT;
    }
    else
    {
        if (!access_ok(VERIFY_READ, arg, _IOC_SIZE(cmd)))
            return -EFAULT;
    }

    switch (cmd)
    {
    case IOCTL_CONVERT:
        return _ioctl_convert(filp->private_data, (fiber_t *)arg);

    case IOCTL_CREATE:
        return _ioctl_create(filp->private_data, (struct fiber_args *)arg);

    case IOCTL_SWITCH:
        return _ioctl_switch(filp->private_data, (fiber_t *)arg);

    case IOCTL_ALLOC:
        return _ioctl_alloc(filp->private_data, (long *)arg);

    case IOCTL_FREE:
        return _ioctl_free(filp->private_data, (long *)arg);

    case IOCTL_GET:
        return _ioctl_get(filp->private_data, (struct fls_args *)arg);

    case IOCTL_SET:
        return _ioctl_set(filp->private_data, (struct fls_args *)arg);

    default:
        return -ENOTTY; //return value is caught and errno set accordingly
    }
}

static int __init fibers_init(void)
{
    int ret;

    //Allocate a major number
    dev_major = register_chrdev(0, KBUILD_MODNAME, &fops);
    if (dev_major < 0)
    {
        ret = dev_major;
        goto fail_regchrdev;
    }

    //create class for the device
    device_class = class_create(THIS_MODULE, "Fibers");
    if (IS_ERR(device_class))
    {
        ret = PTR_ERR(device_class);
        goto fail_classcreate;
    }

    //set permission on device file
    device_class->devnode = fiber_devnode;

    //create device
    device = device_create(device_class, NULL, MKDEV(dev_major, 0), NULL, KBUILD_MODNAME);
    if (IS_ERR(device))
    {
        ret = PTR_ERR(device);
        goto fail_devcreate;
    }

    printk(KERN_NOTICE "%s: Module mounted, device registered with major %d \n", KBUILD_MODNAME, dev_major);

    hash_init(process_table.htable); //should be destroyed?
    spin_lock_init(&process_table.lock);

    ret = register_kprobe(&probe);
    if (ret)
        goto fail_devcreate; //fail chain

    ret = register_kretprobe(&lookup_probe);
    if (ret)
        goto fail_lookup_register;

    ret = register_kretprobe(&readdir_probe);
    if (ret)
        goto fail_readdir_register;

    _set_functions_from_proc();

    return 0;

fail_readdir_register:
    unregister_kretprobe(&lookup_probe);
fail_lookup_register:
    unregister_kprobe(&probe);
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
    unregister_kprobe(&probe);
    unregister_kretprobe(&lookup_probe);
    unregister_kretprobe(&readdir_probe);
    device_destroy(device_class, MKDEV(dev_major, 0));
    class_unregister(device_class);
    class_destroy(device_class);
    unregister_chrdev(dev_major, KBUILD_MODNAME);
    printk(KERN_NOTICE "%s: Module un-mounted\n", KBUILD_MODNAME);
}

int kprobe_cleanup_handler(struct kprobe *p, struct pt_regs *regs)
{
    return _cleanup(&process_table);
}

int kprobe_proc_lookup_handler(struct kretprobe_instance *p, struct pt_regs *regs)
{
    return _lookup_handler(&process_table, p, regs);
}

int kprobe_proc_readdir_handler(struct kretprobe_instance *p, struct pt_regs *regs)
{
    return _readdir_handler(&process_table, p, regs);
}

int fibers_readdir(struct file *file, struct dir_context *ctx)
{
    return fibers_readdir_handler(file, ctx, &process_table);
}

ssize_t fiber_read(struct file *file, char __user *buff, size_t count, loff_t *f_pos)
{
    return fiber_read_handler(file, buff, count, f_pos, &process_table);
}

struct dentry *fibers_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
    return fibers_lookup_handler(dir, dentry, flags, &process_table);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Umberto Mazziotta <mazziotta.1647818@studenti.uniroma1.it>");
MODULE_AUTHOR("Andrea Mastropietro <mastropietro.1652886@studenti.uniroma1.it>");
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);
