#ifndef FKIOCTLS_H
#define FKIOCTLS_H 

#include "Fibers_ktypes.h"

long _ioctl_convert(struct module_hashtable *hashtable, fiber_t *arg, struct task_struct *p);

#endif /* !FKIOCTLS_H */