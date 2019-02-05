#ifndef FKIOCTLS_H
#define FKIOCTLS_H

#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/hashtable.h>
#include <asm/fpu/types.h>

#include "fibers_ktypes.h"
#include "Fibers_ioctls.h"

#define get_cpu_time() (((current->utime) + (current->stime)) / 1000000) //ms

long _ioctl_convert(struct process_active *process, fiber_t *arg);
long _ioctl_create(struct process_active *process, struct fiber_args *args);
long _ioctl_switch(struct process_active *process, fiber_t *usr_id_next);

long _ioctl_alloc(struct process_active *process, long *index);
long _ioctl_free(struct process_active *process, long *index);
long _ioctl_get(struct process_active *process, struct fls_args *);
long _ioctl_set(struct process_active *process, struct fls_args *);

int _cleanup(struct module_hashtable *);

#endif /* !FKIOCTLS_H */