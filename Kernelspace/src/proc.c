#include <linux/slab.h>
#include "proc.h"

static proc_pident_lookup_t origin_proc_pident_lookup;
static proc_pident_readdir_t origin_proc_pident_readdir;

//copyed from proc/base.h
struct file_operations fops = {
    .read = generic_read_dir,
    .iterate_shared = fibers_readdir,
    .llseek = generic_file_llseek,
};

struct inode_operations iops = {
    .lookup = fibers_lookup,
};                           

struct file_operations fiber_ops = {
    .read = fiber_read,
};

struct pid_entry fiber_folder = DIR("fibers", S_IRUGO | S_IXUGO, iops, fops);

void _set_proc_dirent_lookup_from_kprobes(struct kretprobe *kpr)
{
    origin_proc_pident_lookup = (void *)kpr->kp.addr;
}

void _set_proc_dirent_readdir_from_kprobes(struct kretprobe *kpr)
{
    origin_proc_pident_readdir = (void *)kpr->kp.addr;
}

int _lookup_handler(struct module_hashtable *process_table, struct kretprobe_instance *p, struct pt_regs *regs)
{
    struct inode *dir;
    struct dentry *dentry;
    struct pid_entry *ents;
    unsigned int nents;

    unsigned long folder_pid;
    struct process_active *process;
    
    struct kret_data *pdata;
    
    pdata = (struct kret_data *) p->data;
    pdata->ents = NULL;

    dir = (struct inode *)regs->di;
    dentry = (struct dentry *)regs->si;
    ents = (struct pid_entry *)regs->dx;
    nents = (unsigned int)regs->cx;

    if (kstrtoul(dentry->d_name.name, 10, &folder_pid))
        return 0;

    process = find_process(process_table, folder_pid);

    if (!process)
        return 0;

    pdata->ents = kmalloc(sizeof(struct pid_entry) * (nents + 1), GFP_KERNEL);
    memcpy(pdata->ents, ents, sizeof(struct pid_entry) * nents);
    memcpy(&pdata->ents[nents], &fiber_folder, sizeof(struct pid_entry));

    regs->dx = (unsigned long)pdata->ents;
    regs->cx = nents + 1;

    return 0;
}

int kprobe_proc_post_lookup_handler(struct kretprobe_instance *p, struct pt_regs *regs) {
    
    struct kret_data *pdata;
    pdata = (struct kret_data *) p->data;
    if (pdata->ents)
        kfree(pdata->ents);
    return 0;
}

//readdir is used to read directories (OBV)
int _readdir_handler(struct module_hashtable *process_table, struct kretprobe_instance *p, struct pt_regs *regs) //params in di, si, dx, cx
{
    struct file *file;
    struct dir_context *ctx;
    struct pid_entry *ents;
    unsigned int nents;
    unsigned long folder_pid;
    struct process_active *process;

    struct kret_data *pdata;
    
    pdata = (struct kret_data *) p->data;
    pdata->ents = NULL;

    file = (struct file *)regs->di;
    ctx = (struct dir_context *)regs->si;
    ents = (struct pid_entry *)regs->dx;
    nents = (unsigned int)regs->cx;

    if (kstrtoul(file->f_path.dentry->d_name.name, 10, &folder_pid))
        return 0;

    process = find_process(process_table, folder_pid);

    if (!process)
        return 0;

    pdata->ents = kmalloc(sizeof(struct pid_entry) * (nents + 1), GFP_KERNEL);
    memcpy(pdata->ents, ents, sizeof(struct pid_entry) * nents);
    memcpy(&pdata->ents[nents], &fiber_folder, sizeof(struct pid_entry));

    regs->dx = (unsigned long)pdata->ents;
    regs->cx = nents + 1;
    
    return 0;
}

int kprobe_proc_post_readdir_handler(struct kretprobe_instance *p, struct pt_regs *regs) {
    struct kret_data *pdata;
    pdata = (struct kret_data *) p->data;
    if (pdata->ents)
        kfree(pdata->ents);
    return 0;
}

int fibers_readdir_handler(struct file *file, struct dir_context *ctx, struct module_hashtable *process_table)
{

    unsigned long folder_pid;
    struct process_active *process;
    struct pid_entry *ents;
    struct fiber_struct *fiber;
    unsigned int next;
    unsigned int nents;
    int ret_val;

    if (kstrtoul(file->f_path.dentry->d_parent->d_name.name, 10, &folder_pid))
        return 0;

    //may use a lock
    process = find_process(process_table, folder_pid);

    if (!process)
        return 0;


    next = process->next_fid;

    ents = kmalloc(sizeof(struct pid_entry) * next, GFP_KERNEL);
    nents = 0;
    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {
        ents[nents].name = fiber->name;
        ents[nents].len = strlen(fiber->name);
        ents[nents].mode = S_IFREG | S_IRUGO;
        ents[nents].iop = NULL;
        ents[nents].fop = &fiber_ops;

        nents++;
    }

    hlist_for_each_entry(fiber, &process->waiting_fibers, next)
    {
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

struct dentry *fibers_lookup_handler(struct inode *dir, struct dentry *dentry, unsigned int flags, struct module_hashtable *process_table)
{
    unsigned long folder_pid;
    struct process_active *process;
    struct pid_entry *ents;
    struct fiber_struct *fiber;
    unsigned int next;
    unsigned int nents;
    struct dentry *ret_val;

    if (kstrtoul(dentry->d_parent->d_name.name, 10, &folder_pid))
        return NULL;

    process = find_process(process_table, folder_pid);

    if (!process)
        return NULL;

    next = process->next_fid;

    ents = kmalloc(sizeof(struct pid_entry) * next, GFP_KERNEL);
    nents = 0;
    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {
        ents[nents].name = fiber->name;
        ents[nents].len = strlen(fiber->name);
        ents[nents].mode = S_IFREG | S_IRUGO;
        ents[nents].iop = NULL;
        ents[nents].fop = &fiber_ops;

        nents++;
    }

    hlist_for_each_entry(fiber, &process->waiting_fibers, next)
    {
        ents[nents].name = fiber->name;
        ents[nents].len = strlen(fiber->name);
        ents[nents].mode = S_IFREG | S_IRUGO;
        ents[nents].iop = NULL;
        ents[nents].fop = &fiber_ops;

        nents++;
    }

    ret_val = origin_proc_pident_lookup(dir, dentry, ents, nents);

    kfree(ents);

    return ret_val;
}

ssize_t fiber_read_handler(struct file *file, char __user *buff, size_t count, loff_t *f_pos, struct module_hashtable *process_table)
{

    char fiber_info[512];
    unsigned long fiber_id;
    unsigned long pid;
    size_t bytes_written, offset;
    struct process_active *process;
    struct fiber_struct *fiber;

    if (kstrtoul(file->f_path.dentry->d_name.name, 10, &fiber_id))
        return 0;

    if (kstrtoul(file->f_path.dentry->d_parent->d_parent->d_name.name, 10, &pid))
        return 0;

    process = find_process(process_table, pid);

    if (!process)
        return 0;


    hlist_for_each_entry(fiber, &process->running_fibers, next)
    {

        if (fiber->fiber_id == fiber_id)
            goto PRINT;
    }

    hlist_for_each_entry(fiber, &process->waiting_fibers, next)
    {
        if (fiber->fiber_id == fiber_id)
            goto PRINT;
    }
    return 0;

PRINT:

    bytes_written = snprintf(fiber_info, 512, "Status: %s\n"
                                              "Initial entry point: 0x%lX\n"
                                              "Parent thread: %d\n"
                                              "Number of activations: %d\n"
                                              "Number of failed activations: %d\n"
                                              "Total execution time: %llu ms\n",
                             ((fiber->status == 0) ? "WAITING" : "RUNNING"),
                             (unsigned long)fiber->entry_point,
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