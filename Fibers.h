#include <stdbool.h>

#include "Fibers_utypes.h"
#include "Fibers_ioctls.h"

void *ConvertThreadToFiber(void);
void *CreateFiber(size_t stack_size, void (*routine)(void *), void *args);
void SwitchToFiber(void* fiber);

long FlsAlloc(void);
bool FlsFree(long index);
long long FlsGetValue(long index);
void FlsSetValue(long index, long long value);