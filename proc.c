#include "proc.h"
#include <linux/slab.h>

proc_pident_lookup_t    origin_proc_pident_lookup;
proc_pident_readdir_t   origin_proc_pident_readdir;

void _set_proc_dirent_lookup_from_kprobes(struct kprobe *kpr) {
    origin_proc_pident_lookup = (void *) kpr->addr;
}

void _set_proc_dirent_readdir_from_kprobes(struct kprobe *kpr) {
    origin_proc_pident_readdir = (void *) kpr->addr;
}

//copyed from proc/base.h
struct file_operations fops = {
    .read = generic_read_dir,
    .iterate_shared = fibers_readdir, //allow concurrent readdir, here we should create entires under "/fibers/*" how we do this? here we should rely on proc_pident_readdir()
    .llseek = generic_file_llseek,
};

struct inode_operations iops = {
    .lookup = fibers_lookup, //this should rely on proc_pident_lookup()
}; //lookup tells we have other inode to see, what we should do here?

struct file_operations fiber_ops = {
    .read = fiber_read,
};

struct pid_entry fiber_folder = DIR("fibers", S_IRUGO|S_IXUGO, iops, fops);

//for the fiber entries generated with iterate_shared we have to define a read to materialize the information, no inode op since it does not exist

//this is already defined somewhere else
static inline struct process_active* find_process(struct module_hashtable *hashtable, pid_t tgid)
{
    struct process_active   *ret;
    struct hlist_node       *n;
    hash_for_each_possible_safe(hashtable->htable, ret, n, next, tgid)
    {
        if (ret->tgid == tgid)
            return ret;
    }
    
    return NULL;
}

//apparently does nothing
int _lookup_handler(struct module_hashtable* process_table, struct pt_regs* regs)
{   
    /*
    struct inode *dir;
    struct dentry *dentry;
    struct pid_entry* ents;
    unsigned int nents;

    unsigned long folder_pid;
    struct process_active *process;
    struct pid_entry* new_ents;

    dir = (struct inode*) regs->di;
    dentry = (struct dentry*) regs->si;
    ents = (struct pid_entry*) regs->dx; 
    nents = (unsigned int) regs->cx;

    if (kstrtoul(dentry->d_name.name, 10, &folder_pid)) 
        return 0;
    
    process = find_process(process_table, folder_pid);
    
    if (!process)
        return 0;
    
    new_ents = kmalloc(sizeof(struct pid_entry)*(nents+1), GFP_KERNEL);     
    //printk(KERN_NOTICE KBUILD_MODNAME " : pid; %ld process: %d\n", folder_pid, process->tgid);
    memcpy(new_ents, ents, sizeof(struct pid_entry)*nents);
    memcpy(&new_ents[nents], &fiber_folder, sizeof(struct pid_entry));

    regs->dx = new_ents;
    regs->cx = nents+1;

    printk(KERN_NOTICE KBUILD_MODNAME " : _lookup_handler function activated.\n");
    */
    return 0;
}

//readdir is used to read directories (OBV)
int _readdir_handler(struct module_hashtable* process_table, struct pt_regs* regs) //params in di, si, dx, cx 
{
    struct file*file;
    struct dir_context* ctx;
    struct pid_entry* ents;
    unsigned int nents;
    unsigned long folder_pid;
    struct process_active *process;
    struct pid_entry* new_ents;
    
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

    //printk(KERN_NOTICE KBUILD_MODNAME " : _readdir_handler function terminated.\n");
    return 0;
}

int fibers_readdir_handler(struct file *file, struct dir_context *ctx, struct module_hashtable* process_table) {

    unsigned long folder_pid;
    struct process_active* process;
    struct pid_entry* ents;
    struct fiber_struct* fiber;
    unsigned int next;
    unsigned int nents;
    int ret_val;

    if (kstrtoul(file->f_path.dentry->d_parent->d_name.name, 10, &folder_pid)) 
        return 0;
    
    //may use a lock
    process = find_process(process_table, folder_pid);

    if (!process)
    {
        return 0;
    }

    next = process->next_fid;

    ents = kmalloc(sizeof(struct pid_entry)*next, GFP_KERNEL);
    nents = 0;
    hlist_for_each_entry(fiber, &process->running_fibers, next){
        ents[nents].name = fiber->name;
        ents[nents].len = strlen(fiber->name);
        ents[nents].mode = S_IFREG | S_IRUGO;
        ents[nents].iop = NULL;
        ents[nents].fop = &fiber_ops;

        nents++;
    }

    hlist_for_each_entry(fiber, &process->waiting_fibers, next){
        ents[nents].name = fiber->name;
        ents[nents].len = strlen(fiber->name);
        ents[nents].mode = S_IFREG | S_IRUGO;
        ents[nents].iop = NULL;
        ents[nents].fop = &fiber_ops;

        nents++;
    }
    
    ret_val = origin_proc_pident_readdir(file, ctx, ents, nents);

    kfree(ents);

    return ret_val;
}

ssize_t fiber_read_handler(struct file* file, char __user* buff, size_t count, loff_t* f_pos, struct module_hashtable* process_table){
    
    char fiber_info[512];
    unsigned long fiber_id;
    unsigned long pid;
    size_t bytes_written, offset;
    struct process_active* process;
    struct fiber_struct* fiber;

    if (kstrtoul(file->f_path.dentry->d_name.name, 10, &fiber_id)) 
        return 0;

    if (kstrtoul(file->f_path.dentry->d_parent->d_parent->d_name.name, 10, &pid)) 
        return 0;

    process = find_process(process_table, pid);

    if (!process)
    {
        return 0;
    }

    hlist_for_each_entry(fiber, &process->running_fibers, next){
        
        if (fiber->fiber_id == fiber_id) goto PRINT;
        
    }

    hlist_for_each_entry(fiber, &process->waiting_fibers, next){
        if (fiber->fiber_id == fiber_id) goto PRINT;
    }
    return 0;

    PRINT:

    bytes_written = snprintf(fiber_info, 512, "Status: %s\n"
                                    "Initial entry point: 0x%lX\n"
                                    "Parent thread: %d\n"
                                    "Number of activations: %d\n"
                                    "Number of failed activations: %d\n"
                                    "Total execution time: %llu ns\n", 
                                    ((fiber->status == 0) ? "WAITING" : "RUNNING"),
                                    (unsigned long) fiber->entry_point,
                                    fiber->parent_thread, fiber->activations,
                                    fiber->failed_activations,
                                    fiber->execution_time);

    if (*f_pos >= bytes_written)
        return 0;

    offset = (count < bytes_written) ? count : bytes_written;
    if (copy_to_user(buff, fiber_info, offset))
        return -EFAULT;

    *f_pos += offset;
    return offset;
}

struct dentry* fibers_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    return NULL;
}


