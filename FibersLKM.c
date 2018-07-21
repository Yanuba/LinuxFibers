#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h> //for file system operations

#include "FibersLKM.h"


static int __init fibers_init(void) 
{
    printk(KERN_NOTICE "Module mounted");
    return 0;
}

static void __exit fibers_exit(void)
{
    printk(KERN_NOTICE "Module un-mounted");
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Umberto Mazziotta <mazziotta.1647818@studenti.uniroma1.it>");
MODULE_DESCRIPTION("Fibers LKM");

module_init(fibers_init);
module_exit(fibers_exit);