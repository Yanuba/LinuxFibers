#ifndef _PROC_H
#define _PROC_H

#include <linux/fs.h>
#include <linux/kprobes.h>
#include <linux/uaccess.h>

#include "Fibers_kioctls.h"

/**
 * Flieds copied from proc/base.h 
 */
union proc_op {
  int (*proc_get_link)(struct dentry *, struct path *);
  int (*proc_show)(struct seq_file *m, struct pid_namespace *ns, struct pid *pid, struct task_struct *task);
};

struct pid_entry
{
  const char *name;
  unsigned int len;
  umode_t mode;
  const struct inode_operations *iop;
  const struct file_operations *fop;
  union proc_op op;
};

#define NOD(NAME, MODE, IOP, FOP, OP) \
  {                                   \
    .name = (NAME),                   \
    .len = sizeof(NAME) - 1,          \
    .mode = MODE,                     \
    .iop = IOP,                       \
    .fop = FOP,                       \
    .op = OP,                         \
  }

#define DIR(NAME, MODE, iops, fops) \
  NOD(NAME, (S_IFDIR | (MODE)), &iops, &fops, {})

/**/

int kprobe_proc_post_lookup_handler(struct kretprobe_instance *, struct pt_regs *);
int kprobe_proc_post_readdir_handler(struct kretprobe_instance *, struct pt_regs *);

struct kret_data
{
  struct pid_entry *ents;
};

typedef struct dentry *(*proc_pident_lookup_t)(struct inode *, struct dentry *, const struct pid_entry *, unsigned int);
typedef int (*proc_pident_readdir_t)(struct file *, struct dir_context *, const struct pid_entry *, unsigned int);

void _set_functions_from_proc(void);

int _lookup_handler(struct module_hashtable *, struct kretprobe_instance *p, struct pt_regs *);
int _readdir_handler(struct module_hashtable *, struct kretprobe_instance *p, struct pt_regs *);
int fibers_readdir_handler(struct file *file, struct dir_context *ctx, struct module_hashtable *process_table);
int fibers_readdir(struct file *, struct dir_context *);

ssize_t fiber_read(struct file *, char __user *, size_t, loff_t *);
ssize_t fiber_read_handler(struct file *, char __user *, size_t, loff_t *, struct module_hashtable *);

struct dentry *fibers_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
struct dentry *fibers_lookup_handler(struct inode *dir, struct dentry *dentry, unsigned int flags, struct module_hashtable *);

#endif /* !_PROC_H */