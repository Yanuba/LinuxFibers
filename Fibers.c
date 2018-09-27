#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include <pthread.h>

#include <errno.h>

#include "Fibers.h"

/*
 * Must perform a better wrapping for the calls? We could lose errno value.
 * Convert the current thread into a fiber.
 * In case of success is returned an identifier for the new Fiber.
 * In case of Failure NULL is returned.
 * */
void *ConvertThreadToFiber() {
    int ret;
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return NULL;
    }

    fiber_id *id = (fiber_id *) malloc(sizeof(fiber_id)); 

    ret = ioctl(fd, IOCTL_CONVERT, id);
    if (ret) {
        *id = -1;
        //debug stuff TO DO
    }

    close(fd);
    return id;
}

/*
 *@TO_DO
 * */
void *CreateFiber(size_t stack_size, void *(*routine)(void *), void *args) {
    int ret;
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return NULL;
    }

    fiber_id *id = (fiber_id *) malloc(sizeof(fiber_id)); 

    create_args *arguments = (create_args*) malloc(sizeof(s_create_args));
    arguments->routine = routine;
    arguments->routine_args = args;
    
    /* Stack allocation copied from ult*/
    void *stack;
	size_t reminder;
	if (size <= 0) {
		size = SIGSTKSZ;
	}

	// Align the size to the page boundary
	reminder = stack_size % getpagesize();
	if (reminder != 0) {
		stack_size += getpagesize() - reminder;
	}

	posix_memalign(&stack, 16, 4096*2);
	bzero(stack, stack_size);

    arguments->stack_address = stack;;

    ret = ioctl(fd, IOCTL_CREATE, arguments);
    if (ret) *id = -1;    
    else *id = arguments->ret;

    //free(arguments);

    close(fd)
    return id;
}

/*
 *@TO_DO
 * */
void SwitchToFiber(void* fiber) {
    int ret;
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return;
    }

    ret = ioctl(fd, IOCTL_SWITCH, fiber);
    
    //check ret value
    
    close(fd);
    return;
}

/*
 *@TO_DO
 * */
long FlsAlloc(void) {
    return -1;
}

/*
 *@TO_DO
 * */
void FlsFree(long index) {
    return;
}

/*
 *@TO_DO
 * */
void *FlsGetValue(long index){
    return NULL;
}

/*
 *@TO_DO
 * */
void FlsSetValue(long index, void* value){
    return;
}

/******************** 
 *  TEST PROGRAM    *
 ********************/

void * thread_routine(void* arg) {
    fiber_id* fib = ConvertThreadToFiber();
    printf("My pid is %d, the fid get is: %d\n", getpid(), *fib);
    fiber_id* fib2 = ConvertThreadToFiber();
    //printf("My pid is %d, the fid get is: %d\n", getpid(), *fib2);
    return 0;
}

int main() {
    
    pthread_t t1;
    pthread_t t2;
    pthread_t t3;
    
    pthread_create(&t1, NULL, thread_routine, NULL);
    //pthread_create(&t2, NULL, thread_routine, NULL);
    //pthread_create(&t3, NULL, thread_routine, NULL);

    pthread_join(t1, NULL);
    //pthread_join(t2, NULL);
    //pthread_join(t3, NULL);

    //ConvertThreadToFiber();

    return 0;
}
