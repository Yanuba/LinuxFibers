#include "FibersLKM.h"

void *ConvertThreadToFiber(void);
void *CreateFiber(size_t stack_size, void* (*routine)(void *), void *args);
void SwitchToFiber(void* fiber);

long FlsAlloc(void);
void FlsFree(long index);
void *FlsGetValue(long index);
void FlsSetValue(long index, void* value);