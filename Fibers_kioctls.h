#ifndef FKIOCTLS_H
#define FKIOCTLS_H 

#include "Fibers_ktypes.h"

long _ioctl_convert(struct module_hashtable *hashtable, fiber_t *arg, struct task_struct *p);
long _ioctl_create(struct module_hashtable *hashtable, struct fiber_args *args, struct task_struct *p);
long _ioctl_switch(struct module_hashtable *hashtable, fiber_t* usr_id_next, struct task_struct *p);

#endif /* !FKIOCTLS_H */