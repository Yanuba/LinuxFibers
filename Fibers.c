#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include <string.h>
#include <pthread.h>
#include <errno.h>

#include "Fibers.h"
#include "Fibers_ioctls.h"
/*
 * Must perform a better wrapping for the calls? We could lose errno value.
 * Convert the current thread into a fiber.
 * In case of success is returned an identifier for the new Fiber.
 * In case of Failure NULL is returned.
 * */
void *ConvertThreadToFiber() {
    int ret;
    int fd = open("/dev/FibersModule", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return NULL;
    }

    fiber_t *id = (fiber_t *) malloc(sizeof(fiber_t)); 

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
    int fd = open("/dev/FibersModule", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return NULL;
    }

    fiber_t *id = (fiber_t *) malloc(sizeof(fiber_t)); 

    struct s_create_args *arguments = (struct s_create_args*) malloc(sizeof(struct s_create_args));
    arguments->routine = routine;
    arguments->routine_args = args;
    
    /* Stack allocation copied from ult*/
    void *stack;
	size_t reminder;
	if (stack_size <= 0) {
		stack_size = 8192;
	}

	// Align the size to the page boundary
	reminder = stack_size % getpagesize();
	if (reminder != 0) {
		stack_size += getpagesize() - reminder;
	}

	posix_memalign(&stack, 16, 4096*2);
	bzero(stack, stack_size);

    arguments->stack_address = stack+stack_size-8; //-8 since stack expect a return address

    ret = ioctl(fd, IOCTL_CREATE, arguments);
    if (ret) *id = -1;    
    else *id = arguments->ret;

    free(arguments);

    close(fd);
    return id;
}

/*
 *@TO_DO
 * */
void SwitchToFiber(void* fiber) {
    int ret;
    int fd = open("/dev/FibersModule", O_RDONLY);
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
fiber_t* fib;

void * thread_routine2(void* arg) {
    printf("FIIIIIIIIIIIIIBER\n");
    
    double l1 = 96;
    double l2 = 83.9;
    double l3 = l1*l2;
    
    unsigned long x = (unsigned long) arg;
    printf("%lu", x);
    
    
    
    printf("%f", l3);



    printf("FIIIIIIIIIIIIIBER\n");
    
    SwitchToFiber(fib);
    
    printf("FIIIIIIIIIIIIIBER22\n");

    SwitchToFiber(fib);

    while(1) {printf("%f, %lu\n", l3, x);};

}

void * thread_routine(void* arg) {
    fib = ConvertThreadToFiber();
    printf("My pid is %d, the fid get is: %d\n", getpid(), *fib);
    fiber_t* fib2 = ConvertThreadToFiber();
    //printf("My pid is %d, the fid get is: %d\n", getpid(), *fib2);
    fib2 = CreateFiber(2*4096, thread_routine2, (void *) (unsigned long) 50);
    printf("My pid is %d, the fid get is: %d\n", getpid(), *fib2);

    SwitchToFiber(fib2);
    printf("My pid is bacl from hell");
    SwitchToFiber(fib2);
    printf("My pid is bacl from hello");
    //fiber_id* fib3 = CreateFiber(4096, thread_routine2, (void *) (unsigned long) 50);
    //printf("My pid is %d, the fid get is: %d\n", getpid(), *fib3);
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
