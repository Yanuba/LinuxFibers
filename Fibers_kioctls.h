#ifndef FKIOCTLS_H
#define FKIOCTLS_H 

#include "Fibers_ktypes.h"

long _ioctl_convert(struct module_hashtable *hashtable, fiber_t *arg);
long _ioctl_create(struct module_hashtable *hashtable, struct fiber_args *args);
long _ioctl_switch(struct module_hashtable *hashtable, fiber_t* usr_id_next);

long _ioctl_alloc(struct module_hashtable *hashtable, long* index);
//long _ioctl_free();
//long _ioctl_get();
//long _ioctl_set();

#endif /* !FKIOCTLS_H */