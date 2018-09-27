#include "FibersLKM.h" //Expose the fiber_struct to user space, but we cannot manipulate it

typedef struct s_create_args {
    void* stack_address;
    void* (*routine)(void *);
    void* routine_args;
} creat_args;

typedef struct fiber_struct_usr fiber_t;

void *ConvertThreadToFiber(void);
void *CreateFiber(size_t stack_size, void* (*routine)(void *), void *args);
void SwitchToFiber(void* fiber);

long FlsAlloc(void);
void FlsFree(long index);
void *FlsGetValue(long index);
void FlsSetValue(long index, void* value);