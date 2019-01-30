#ifndef FKIOCTLS_H
#define FKIOCTLS_H 

#include "Fibers_ktypes.h"

long _ioctl_convert(struct process_active *process, fiber_t *arg);
long _ioctl_create(struct process_active *process, struct fiber_args *args);
long _ioctl_switch(struct process_active *process, fiber_t* usr_id_next);

long _ioctl_alloc(struct process_active *process, long* index);
long _ioctl_free(struct process_active *process, long* index);
long _ioctl_get(struct process_active *process, struct fls_args *);
long _ioctl_set(struct process_active *process, struct fls_args *);

int _cleanup(struct module_hashtable*);

#endif /* !FKIOCTLS_H */