#include "proc.h"

int _lookup_handler(struct module_hashtable* process_table, struct pt_regs* regs)
{
    printk(KERN_NOTICE KBUILD_MODNAME " : _lookup_handler function activated.\n");
    return 0;
}

int _readdir_handler(struct module_hashtable* process_table, struct pt_regs* regs)
{
    printk(KERN_NOTICE KBUILD_MODNAME " : _readdir_handler function activated.\n");
    return 0;
}