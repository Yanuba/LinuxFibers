/*
 * How to deal with errno? 
 * */ 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <strings.h>

#include "Fibers.h"
#include "Fibers_ioctls.h"

int _FIBER_DESCRIPTOR = -1;
bool _DESCRIPTOR_GUARD = false;

/*
 * Convert the current thread into a fiber.
 * In case of success is returned an identifier for the new Fiber.
 * In case of Failure a non valid fiber identifier is returned, all calls will fail.
 * */
void *ConvertThreadToFiber() 
{    
    fiber_t *id;
    
    id = (fiber_t *) malloc(sizeof(fiber_t)); 

    if (_DESCRIPTOR_GUARD == false) 
    {
        _FIBER_DESCRIPTOR = open("/dev/FibersModule", O_RDONLY);
        if (_FIBER_DESCRIPTOR == -1) 
        {
            *id = -1;
            return id;
        }
        else _DESCRIPTOR_GUARD = true;
    }
    
    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_CONVERT, id)) 
    {
        *id = -1;
    }

    return id;
}

/*
 * Creates a new Fiber context
 * In case if success an identifier for the new fiber is returned.
 * In case of failure a non vaild fiber identifier is returend, all calls on it will fail.
 * */
void *CreateFiber(size_t stack_size, void (*routine)(void *), void *args) 
{
    struct fiber_args   arguments;
    fiber_t             *id;
    
    void *stack;
	size_t reminder;

    id = (fiber_t *) malloc(sizeof(fiber_t)); 

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

    arguments.stack_address = stack+stack_size-8; //-8 since stack expect a return address
    //((unsigned long *) arguments.stack_address)[-1] = (unsigned long) &foo; 

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_CREATE, &arguments)) 
        *id = -1;    
    else 
        *id = arguments.ret;

    return id;
}

/*
 * Switches the execution context to a different Fiber.
 * It can fail if switching to a Fiber which is already active
 * */
bool SwitchToFiber(void* fiber) 
{
    if(ioctl(_FIBER_DESCRIPTOR, IOCTL_SWITCH, fiber))
        return false;
    return true;
}

/*
 * Allocates a FLS Index.
 * In case of error, returns -1, which is not valid index
 * */
long FlsAlloc(void) 
{
    long index;
    
    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_ALLOC, &index))
        index = -1;
    return index;
}

/*
 *  Free an FLS Index
 *  Return false in case of error 
 * */
bool FlsFree(long index) {

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_FREE, &index))
        return false;
    return true;
}

/*
 *  GetValue associated to FLS Index
 *  returns -1 in case of error
 * */
long long FlsGetValue(long index){

    struct fls_args args;
    args.index = index;

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_GET, &args)) 
        return -1;

    return args.value;
}

/*
 *@TO_DO
 * */
bool FlsSetValue(long index, long long value){
    struct fls_args args;
    args.index = index;
    args.value = value;

    if (ioctl(_FIBER_DESCRIPTOR, IOCTL_SET, &args))
        return false;
    return true;
}

