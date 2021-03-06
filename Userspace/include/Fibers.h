#ifndef USERSPACE
#define USERSPACE

#include <stdbool.h>
#include <linux/ioctl.h>

#define IOCTL_MAGIC_NUM ']' //it is not used by any device driver (it should be safe)

typedef pid_t fiber_t;

struct fiber_args
{
    void *stack_address;
    void (*routine)(void *);
    void *routine_args;
    fiber_t ret;
};

struct fls_args
{
    unsigned long index;
    long long value;
};

//IOR return data to userland, IOW read data from userland
#define IOCTL_CONVERT _IOR(IOCTL_MAGIC_NUM, 0, fiber_t)
#define IOCTL_CREATE _IOWR(IOCTL_MAGIC_NUM, 1, struct fiber_args)
#define IOCTL_SWITCH _IOWR(IOCTL_MAGIC_NUM, 2, fiber_t)

//FLS IOCTL
#define IOCTL_ALLOC _IOR(IOCTL_MAGIC_NUM, 3, long)
#define IOCTL_FREE _IOW(IOCTL_MAGIC_NUM, 4, long)
#define IOCTL_GET _IOWR(IOCTL_MAGIC_NUM, 5, struct fls_args)
#define IOCTL_SET _IOW(IOCTL_MAGIC_NUM, 6, struct fls_args)

void *ConvertThreadToFiber(void);
void *CreateFiber(size_t stack_size, void (*routine)(void *), void *args);
bool SwitchToFiber(void *fiber);

long FlsAlloc(void);
bool FlsFree(long index);
long long FlsGetValue(long index);
bool FlsSetValue(long index, long long value);

#endif /* !USERSPACE */
