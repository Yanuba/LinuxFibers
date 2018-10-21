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
    int     ret;
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
    
    ret = ioctl(_FIBER_DESCRIPTOR, IOCTL_CONVERT, id);
    if (ret) 
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
void *CreateFiber(size_t stack_size, void *(*routine)(void *), void *args) 
{
    int                 ret;
    fiber_t             *id;
    struct fiber_args   *arguments;
    
    id = (fiber_t *) malloc(sizeof(fiber_t)); 

    arguments = (struct fiber_args*) malloc(sizeof(struct fiber_args));
    arguments->routine = routine;
    arguments->routine_args = args;
    
    void *stack;
	size_t reminder;
	if (stack_size <= 0) 
		stack_size = 8192;

	// Align the size to the page boundary
	reminder = stack_size % getpagesize();
	if (reminder != 0) 
		stack_size += getpagesize() - reminder;

	posix_memalign(&stack, 16, stack_size);
	bzero(stack, stack_size);

    arguments->stack_address = stack+stack_size-8; //-8 since stack expect a return address

    ret = ioctl(_FIBER_DESCRIPTOR, IOCTL_CREATE, arguments);
    if (ret) 
        *id = -1;    
    else 
        *id = arguments->ret;

    free(arguments);
    return id;
}

/*
 * Switches the execution context to a different Fiber.
 * It can fail if switching to a Fiber which is already active
 * */
void SwitchToFiber(void* fiber) 
{
    int ret;

    ret = ioctl(_FIBER_DESCRIPTOR, IOCTL_SWITCH, fiber);
    
    return;
}

/*
 * Allocates a FLS Index.
 * In case of error, returns -1, which is not valid index
 * */
long FlsAlloc(void) 
{
    int ret;
    long index;
    
    ret = ioctl(_FIBER_DESCRIPTOR, IOCTL_ALLOC, &index);
    if (ret)
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
 *@TO_DO
 * */
long long FlsGetValue(long index){
    return -1;
}

/*
 *@TO_DO
 * */
void FlsSetValue(long index, long long value){
    return;
}

