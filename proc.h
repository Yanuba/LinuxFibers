#ifndef _PROC_H
#define _PROC_H

#include <linux/fs.h> //struct file, struct dir context, 
#include <linux/kprobes.h>
#include <linux/uaccess.h>

#include "Fibers_kioctls.h"

//creates entries under /proc/PID/fibers
//int our_proc_readdir(struct file *file, struct dir_context *ctx);

//dentries to be shown in /proc/PID/fibers/
//struct dentry *our_proc_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)

/** 
 * at the end we don't care of this
 * We leave this here so the compiler doesn't cry
 */
union proc_op {
  int (*proc_get_link)(struct dentry *, struct path *);
  int (*proc_show)(struct seq_file *m, struct pid_namespace *ns, struct pid *pid, struct task_struct *task);
};

struct pid_entry {
  const char *name;
  unsigned int len;
  umode_t mode;
  const struct inode_operations *iop;
  const struct file_operations *fop;
  union proc_op op;
};

//macro to define a directory in proc
#define NOD(NAME, MODE, IOP, FOP, OP) { \
  .name = (NAME),                       \
  .len  = sizeof(NAME) - 1,             \
  .mode = MODE,                         \
  .iop  = IOP,                          \
  .fop  = FOP,                          \
  .op   = OP,                           \
}

#define DIR(NAME, MODE, iops, fops)  \
  NOD(NAME, (S_IFDIR|(MODE)), &iops, &fops, {} )

//for fop we have to implement readdir
//for iops we have to implement the lookup in order to enumerate the files in the folder
//      there we allocate the actual files (dentries) to be shown undef fibers
//          of these we have to implement a read to show our informations

//we want to probe these two
//static int proc_pident_readdir(struct file *file, struct dir_context *ctx, const struct pid_entry *ents, unsigned int nents)
//static struct dentry *proc_pident_lookup(struct inode *dir, struct dentry *dentry, const struct pid_entry *ents, unsigned int nents)

typedef struct  dentry *(*proc_pident_lookup_t) (struct inode *, struct dentry *, const struct pid_entry *, unsigned int);
typedef int (*proc_pident_readdir_t) (struct file *, struct dir_context *, const struct pid_entry *, unsigned int);

void _set_proc_dirent_lookup_from_kprobes(struct kprobe*);
void _set_proc_dirent_readdir_from_kprobes(struct kprobe*);

int _lookup_handler(struct module_hashtable*, struct pt_regs*);
int _readdir_handler(struct module_hashtable*, struct pt_regs*);
int fibers_readdir_handler(struct file *file, struct dir_context *ctx, struct module_hashtable* process_table);

int fibers_readdir(struct file *, struct dir_context *);
ssize_t fiber_read(struct file *, char __user *, size_t, loff_t *);
ssize_t fiber_read_handler(struct file *, char __user *, size_t, loff_t *, struct module_hashtable*);
struct dentry* fibers_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);

#endif /* !_PROC_H */