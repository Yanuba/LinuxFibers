#pragma once

#include <linux/ioctl.h>
 
#define IOCTL_MAGIC_NUM ']' //it is not used by any device driver (it should be safe)

#define IOCTL_EX _IO(IOCTL_MAGIC_NUM, 100)

struct fiber_struct;

/*

#define ConvertThreadToFiber() ult_convert()
#define CreateFiber(dwStackSize, lpStartAddress, lpParameter) ult_creat(dwStackSize, lpStartAddress, lpParameter)
#define SwitchToFiber(lpFiber) ult_switch_to(lpFiber)
#define FlsAlloc(lpCallback) fls_alloc()
#define FlsFree(dwFlsIndex)	fls_free(dwFlsIndex)
#define FlsGetValue(dwFlsIndex) fls_get(dwFlsIndex)
#define FlsSetValue(dwFlsIndex, lpFlsData) fls_set((dwFlsIndex), (long long)(lpFlsData))

#else


// TODO:
// Here you should point to the invocation of your code!
// See README.md for further details.

#define ConvertThreadToFiber() {}
#define CreateFiber(dwStackSize, lpStartAddress, lpParameter) {}
#define SwitchToFiber(lpFiber) {}
#define FlsAlloc(lpCallback) {}
#define FlsFree(dwFlsIndex) {}
#define FlsGetValue(dwFlsIndex) {}
#define FlsSetValue(dwFlsIndex, lpFlsData) {}

*/

//Fibers IOCTL

#define IOCTL_CONVERT _IOR(IOCTL_MAGIC_NUM, 0, /*Type passed here*/long) //ConvertThreadToFiber()
#define IOCTL_CREATE _IOW(IOCTL_MAGIC_NUM, 1, /*Type passed here*/long) //CreateFiber(...)
#define IOCTL_SWITCH _IOW(IOCTL_MAGIC_NUM, 2, /*Type passed here*/long) //SwitchToFiber(...)

//FLS IOCTL

#define IOCTL_ALLOC _IO(IOCTL_MAGIC_NUM, 3) //FlsAlloc(...)
#define IOCTL_FREE _IOW(IOCTL_MAGIC_NUM, 4, /*Type passed here*/long) //FlsFree(...)
#define IOCTL_GET _IOWR(IOCTL_MAGIC_NUM, 5, /*Type passed here*/long) //FlsGetValue(...)
#define IOCTL_SET _IOW(IOCTL_MAGIC_NUM, 6, /*Type passed here*/long) //FlsSetValue(...)


/*
struct fiber_struct {
    short status; //1 running, 0 not running 
    unsigned long entry_point; //addess of the function executed
    struct pt_regs cpu_context;
    long activations;
    long failed_activations;
    long time_elapsed; //how we compute this?
    // Other Parameters?
    //
    //
    //
    //
    //
}
*/