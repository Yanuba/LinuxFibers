#ifndef _PROC_H
#define _PROC_H

#include <linux/fs.h> //struct file, struct dir context, 
#include "Fibers_kioctls.h"

int _lookup_handler(struct module_hashtable*, struct pt_regs*);
int _readdir_handler(struct module_hashtable*, struct pt_regs*);
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

#endif /* !_PROC_H */