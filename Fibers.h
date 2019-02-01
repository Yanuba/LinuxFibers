#ifndef USERSPACE
#define USERSPACE

#include <stdbool.h>

#include "Fibers_ioctls.h"

void *ConvertThreadToFiber(void);
void *CreateFiber(size_t stack_size, void (*routine)(void *), void *args);
bool SwitchToFiber(void* fiber);

long FlsAlloc(void);
bool FlsFree(long index);
long long FlsGetValue(long index);
bool FlsSetValue(long index, long long value);

#endif /* !USERSPACE */