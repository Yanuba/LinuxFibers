#include <linux/module.h>
#include <linux/device.h>
#include <linux/uaccess.h>

#include <linux/hashtable.h>

//for pt regs and fpu
#include <linux/ptrace.h>
#include <linux/sched/task_stack.h>
#include <asm/fpu/internal.h>

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

    switch(cmd) {        
        case IOCTL_CONVERT:
            printk(KERN_NOTICE "%s: ConvertThreadToFiber() called by thread %d of process %d\n", KBUILD_MODNAME, task_pid_nr(caller), task_tgid_nr(caller));
            
            if (!access_ok(VERIFY_WRITE, arg, sizeof(fiber_t))) {
                printk(KERN_NOTICE "%s:  ConvertThreadToFiber() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            return _ioctl_convert(&process_table, (fiber_t *) arg, caller);

        case IOCTL_CREATE:
            printk(KERN_NOTICE "%s: CreateFiber() called by thread %d of process %d\n", KBUILD_MODNAME, task_pid_nr(caller), task_tgid_nr(caller));

            if (!access_ok(VERIFY_WRITE, arg, sizeof(struct fiber_args))) {
                printk(KERN_NOTICE "%s:  CreateFiber() cannot return data to userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            return _ioctl_create(&process_table, (struct fiber_args*) arg, caller);

        case IOCTL_SWITCH:
            if (!access_ok(VERIFY_READ, arg, sizeof(fiber_t))) {
                printk(KERN_NOTICE "%s:  SwitchToFiber() cannot read data from userspace\n", KBUILD_MODNAME);
                return -EFAULT; //Is this correct?
            }

            return _ioctl_switch(&process_table, (fiber_t *) arg, caller);
//            swtich_fiber_to = NULL;

            

//            printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d of process %d\n", KBUILD_MODNAME, task_pid_nr(caller), task_tgid_nr(caller));
            
 //           swtich_to_me_id = kmalloc(sizeof(fiber_t), GFP_KERNEL);

//            if (copy_from_user((void *) swtich_to_me_id, (void *) arg, sizeof(fiber_t))) {
//                printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Cannot copy args\n", KBUILD_MODNAME, task_pid_nr(caller));
//                kfree(swtich_to_me_id);
//                return -ENOTTY; //?
//            }
            

//            hash_for_each_possible(process_table.htable, process_cursor, other, task_tgid_nr(caller)) {
//                if (process_cursor->task_pid_nr(caller) == task_tgid_nr(caller)) {
//                    hlist_for_each(hlist_cursor, &process_cursor->waiting_fibers) {
//                        fiber_cursor = hlist_entry(hlist_cursor, struct fiber_struct, next);
//                       if (fiber_cursor->fiber_id == *swtich_to_me_id) {
//                            printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, found fiber to switch TO\n", KBUILD_MODNAME, task_pid_nr(caller));
//                            swtich_fiber_to = fiber_cursor;
//                        }
//                    }
                    //MAYBE WE HAVE FOUND THE FIBER WE WANT TO SEITCH TO

//                    hlist_for_each(hlist_cursor, &process_cursor->running_fibers) {
//                        fiber_cursor = hlist_entry(hlist_cursor, struct fiber_struct, next);
//                        if (fiber_cursor->fiber_id == *swtich_to_me_id) {
//                            printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Trying to switch to an active fiber\n", KBUILD_MODNAME, task_pid_nr(caller));
//                            fiber_cursor->failed_activations += 1;
//                            return -ENOTTY;
//                        }
//                        else if (fiber_cursor->thread_on == task_pid_nr(caller)) {
                            //found fiber
//                            if (swtich_fiber_to != NULL) {
//                                printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Switching\n", KBUILD_MODNAME, task_pid_nr(caller));

//                                printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Switching from %d\n", KBUILD_MODNAME, task_pid_nr(caller), fiber_cursor->fiber_id);
//                                printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Switching to %d\n", KBUILD_MODNAME, task_pid_nr(caller), swtich_fiber_to->fiber_id);

//                                preempt_disable();
                                //do the switch
                                //save_cpu
//                                fpu__save(&(fiber_cursor->fpu_regs));
//                                (void) memcpy(&(fiber_cursor->regs), task_pt_regs(current), sizeof(struct pt_regs));
                                
                                //restore_cpu
//                                (void) memcpy(task_pt_regs(current), &(swtich_fiber_to->regs),sizeof(struct pt_regs));
//                                fpu__restore(&(swtich_fiber_to->fpu_regs)); //must be here or before?
//                                preempt_enable();
                                
//                                printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Finished\n", KBUILD_MODNAME, task_pid_nr(caller));
                                //increase stats
//                                swtich_fiber_to->activations += 1;
//                                swtich_fiber_to->status = FIBER_RUNNING;
//                                fiber_cursor->status = FIBER_WAITING;
//                               hlist_del(&swtich_fiber_to->next);
//                                hlist_del(&fiber_cursor->next);

//                                hlist_add_head(&swtich_fiber_to->next, &process_cursor->running_fibers);
//                                hlist_add_head(&fiber_cursor->next, &process_cursor->waiting_fibers);
//                                printk(KERN_NOTICE "%s: SwitchToFiber() called by thread %d, Updated status\n", KBUILD_MODNAME, task_pid_nr(caller));

//                                return 0;
//                            }
//                        } 
//                    }

//                }
//            }
            
            /*
            SwitchToFiber(): switches the execution context
            (in the caller thread) to a different Fiber (it can fail if
            switching to a Fiber which is already active)
            */   
//            return -ENOTTY;

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
            FlsSetValue() : Sets a value associated with a FLS index (a long)
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
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);
