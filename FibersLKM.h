#pragma once

#include <linux/ioctl.h>

#define FIBER_RUNNING 1
#define FIBER_WAITING 0

//struct used to expose information needed to use the module

typedef pid_t fiber_id ;

struct s_create_args{
    void* stack_address;
    void* (*routine)(void *);
    void* routine_args;
    fiber_id ret;
};



#define IOCTL_MAGIC_NUM ']' //it is not used by any device driver (it should be safe)

//Fibers IOCTL

//IOR return data to userland, IOW read data from userland

#define IOCTL_CONVERT _IOR(IOCTL_MAGIC_NUM, 0, unsigned long /*Or type passed here*/) //ConvertThreadToFiber()
#define IOCTL_CREATE _IOWR(IOCTL_MAGIC_NUM, 1, unsigned long /*Or type passed here*/) //CreateFiber(...)
#define IOCTL_SWITCH _IOWR(IOCTL_MAGIC_NUM, 2, unsigned long /*Or type passed here*/) //SwitchToFiber(...)

//FLS IOCTL

#define IOCTL_ALLOC _IO(IOCTL_MAGIC_NUM, 3) //FlsAlloc(...)
#define IOCTL_FREE _IOW(IOCTL_MAGIC_NUM, 4, unsigned long /*Or type passed here*/) //FlsFree(...)
#define IOCTL_GET _IOWR(IOCTL_MAGIC_NUM, 5, unsigned long /*Or type passed here*/) //FlsGetValue(...)
#define IOCTL_SET _IOW(IOCTL_MAGIC_NUM, 6, unsigned long /*Or type passed here*/) //FlsSetValue(...)
