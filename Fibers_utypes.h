#ifndef FUTYPES_H
#define FUTYPES_H

typedef pid_t fiber_t;

struct fiber_args
{
    void* stack_address;
    void* (*routine)(void *);
    void* routine_args;
    fiber_t ret;
};

union fls_args
{
    unsigned long   index;
    long long       value;        
};

#endif  /* !FUTYPES_H*/