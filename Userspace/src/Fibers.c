#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <strings.h>

#include "Fibers.h"

static pid_t _CALLER_PID;
static int _FIBER_DESCRIPTOR = -1;
static bool _DESCRIPTOR_GUARD = false;

/**
 * ConvertThreadToFiber - Convert current thred into a fiber
 */
void *ConvertThreadToFiber()
{
    fiber_t *id;

    id = (fiber_t *)malloc(sizeof(fiber_t));

    if (_DESCRIPTOR_GUARD == false)
    {
        _FIBER_DESCRIPTOR = open("/dev/FibersModule", O_RDONLY);
        if (_FIBER_DESCRIPTOR == -1)
        {
            *id = -1;
            return id;
        }
        else
        {
            _CALLER_PID = getpid();
            _DESCRIPTOR_GUARD = true;
        }
    }
    else if (_CALLER_PID != getpid())
    {
        close(_FIBER_DESCRIPTOR);
        _FIBER_DESCRIPTOR = open("/dev/FibersModule", O_RDONLY);
        if (_FIBER_DESCRIPTOR == -1)
        {
            _DESCRIPTOR_GUARD = false;
            *id = -1;
            return id;
        }
        else
        {
            _CALLER_PID = getpid();
            _DESCRIPTOR_GUARD = true;
        }
    }

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_CONVERT, id))
    {
        *id = -1;
    }

    return id;
}

/**
 * CreateFiber - creates a new fiber
 */
void *CreateFiber(size_t stack_size, void (*routine)(void *), void *args)
{
    struct fiber_args arguments;
    fiber_t *id;

    void *stack;
    size_t reminder;

    id = (fiber_t *)malloc(sizeof(fiber_t));

    arguments.routine = routine;
    arguments.routine_args = args;

    if (stack_size <= 0)
        stack_size = 8192;

    // Align the size to the page boundary
    reminder = stack_size % getpagesize();
    if (reminder != 0)
        stack_size += getpagesize() - reminder;

    if (posix_memalign(&stack, 16, stack_size))
    {
        //cannot allocate stack
        *id = -1;
        return id;
    }
    bzero(stack, stack_size);

    arguments.stack_address = stack + stack_size - 8; //-8 since at top of stack the kernel expect a return address
    //((unsigned long *) arguments.stack_address)[-1] = (unsigned long) &foo;

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_CREATE, &arguments))
        *id = -1;
    else
        *id = arguments.ret;

    return id;
}

/**
 * SwitchToFiber - Switches the execution context to a different Fiber.
 * It can fail if switching to a Fiber which is already active
 */
bool SwitchToFiber(void *fiber)
{
    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_SWITCH, fiber))
        return false;
    return true;
}

/**
 * FlsAlloc - Allocates a FLS Index.
 * In case of error, returns -1, which is not valid index
 */
long FlsAlloc(void)
{
    long index;

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_ALLOC, &index))
        index = -1;
    return index;
}

/**
 * FlsFree - Free an FLS Index
 */
bool FlsFree(long index)
{

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_FREE, &index))
        return false;
    return true;
}

/**
 *  FlsGetValue - GetValue associated to FLS Index
 */
long long FlsGetValue(long index)
{

    struct fls_args args;
    args.index = index;

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_GET, &args))
        return -1;

    return args.value;
}

/**
 * FlsSetValue - Set a value of an allocated FLS index
 */
bool FlsSetValue(long index, long long value)
{
    struct fls_args args;
    args.index = index;
    args.value = value;

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_SET, &args))
        return false;
    return true;
}
