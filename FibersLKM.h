#pragma once

#include <linux/ioctl.h>

struct fiber_struct {
    /* How we save the context? pt_regs? To perform contex switch it may be useful use thread_struct or tss_struct */

    /* Fields to be exposed in proc */
    short   status;                 //The fiber is running or not?
    void*   (*entry_point)(void*);   //entry point of the fiber
    pid_t   parent_thread;          //pid of the thread that created the fiber
    int     activations;            //the number of current activations of the Fiber
    int     failed_activations;     //the number of failed activations
    long    execution_time;         //total execution time in that Fiber context
};

//struct used to expose information needed to use the module
struct fiber_struct_usr {
    int info; //Used just for a try
};

#define IOCTL_MAGIC_NUM ']' //it is not used by any device driver (it should be safe)

//Fibers IOCTL

#define IOCTL_CONVERT _IOR(IOCTL_MAGIC_NUM, 0, unsigned long /*Or type passed here*/) //ConvertThreadToFiber()
#define IOCTL_CREATE _IOW(IOCTL_MAGIC_NUM, 1, unsigned long /*Or type passed here*/) //CreateFiber(...)
#define IOCTL_SWITCH _IOW(IOCTL_MAGIC_NUM, 2, unsigned long /*Or type passed here*/) //SwitchToFiber(...)

//FLS IOCTL

#define IOCTL_ALLOC _IO(IOCTL_MAGIC_NUM, 3) //FlsAlloc(...)
#define IOCTL_FREE _IOW(IOCTL_MAGIC_NUM, 4, unsigned long /*Or type passed here*/) //FlsFree(...)
#define IOCTL_GET _IOWR(IOCTL_MAGIC_NUM, 5, unsigned long /*Or type passed here*/) //FlsGetValue(...)
#define IOCTL_SET _IOW(IOCTL_MAGIC_NUM, 6, unsigned long /*Or type passed here*/) //FlsSetValue(...)
