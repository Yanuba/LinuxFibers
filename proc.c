#include "proc.h"
#include <linux/slab.h>


struct file_operations fops;
struct inode_operations iops;

static inline struct process_active* find_process(struct module_hashtable *hashtable, pid_t tgid)
{
    struct process_active   *ret;
    struct hlist_node       *n;
    hash_for_each_possible_safe(hashtable->htable, ret, n, next, tgid)
    {
        
        if (ret->tgid == tgid) {
            
            return ret;
        }           
    }
    
    return NULL;
}

int _lookup_handler(struct module_hashtable* process_table, struct pt_regs* regs)
{
    //printk(KERN_NOTICE KBUILD_MODNAME " : _lookup_handler function activated.\n");
    return 0;
}

int _readdir_handler(struct module_hashtable* process_table, struct pt_regs* regs) //params in di, si, dx, cx 
{
    struct file* file;
    struct dir_context* ctx;
    struct pid_entry* ents;
    unsigned int nents;
    unsigned long folder_pid;
    struct process_active *process;
    struct pid_entry* new_ents;
    struct pid_entry fiber_folder = DIR("Fibers", S_IRUGO|S_IXUGO, iops, fops);

    file = (struct file*) regs->di;
    ctx = (struct dir_context*) regs->si;
    ents = (struct pid_entry*) regs->dx; 
    nents = (unsigned int) regs->cx;

    if (kstrtoul(file->f_path.dentry->d_name.name, 10, &folder_pid)) 
        return 0;
    
    process = find_process(process_table, folder_pid);
    
    

    if (!process)
    {
        //printk(KERN_NOTICE KBUILD_MODNAME " : Stamo naa folder sbajata porcammerda.\n");
        return 0;
    }

    new_ents = kmalloc(sizeof(struct pid_entry)*(nents+1), GFP_KERNEL);     
    //printk(KERN_NOTICE KBUILD_MODNAME " : pid; %ld process: %d\n", folder_pid, process->tgid);
    memcpy(new_ents, ents, sizeof(struct pid_entry)*nents);
    memcpy(&new_ents[nents], &fiber_folder, sizeof(struct pid_entry));

    regs->dx = new_ents;
    regs->cx = nents+1;

    printk(KERN_NOTICE KBUILD_MODNAME " : _readdir_handler function terminated.\n");
    return 0;
}

