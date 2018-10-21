#ifndef FIOCTLS_H
#define FIOCTLS_H

#include <linux/ioctl.h>

#define IOCTL_MAGIC_NUM ']' //it is not used by any device driver (it should be safe)

//IOR return data to userland, IOW read data from userland
#define IOCTL_CONVERT _IOR(IOCTL_MAGIC_NUM, 0, unsigned long /*Or type passed here*/) //ConvertThreadToFiber()
#define IOCTL_CREATE _IOWR(IOCTL_MAGIC_NUM, 1, unsigned long /*Or type passed here*/) //CreateFiber(...)
#define IOCTL_SWITCH _IOWR(IOCTL_MAGIC_NUM, 2, unsigned long /*Or type passed here*/) //SwitchToFiber(...)

//FLS IOCTL
#define IOCTL_ALLOC _IOR (IOCTL_MAGIC_NUM, 3, unsigned long /*Or type passed here*/) //FlsAlloc(...)
#define IOCTL_FREE  _IOW (IOCTL_MAGIC_NUM, 4, unsigned long /*Or type passed here*/) //FlsFree(...)
#define IOCTL_GET   _IOWR(IOCTL_MAGIC_NUM, 5, unsigned long /*Or type passed here*/) //FlsGetValue(...)
#define IOCTL_SET   _IOW (IOCTL_MAGIC_NUM, 6, unsigned long /*Or type passed here*/) //FlsSetValue(...)

#endif /* !FIOCTLS_H */