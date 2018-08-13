#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include <pthread.h>

#include "Fibers.h"

/*
 *@TO_DO
 * */
void *ConvertThreadToFiber() {
    int ret;
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return NULL;
    }

    fiber_t *ctx = (fiber_t *) malloc(sizeof(fiber_t)); 

    ret = ioctl(fd, IOCTL_CONVERT, ctx);
    
    //check ret value    
    //add an atexit() in order to issue a call in order to cleanup the module when the process exits?

    close(fd);
    return ctx;
}

/*
 *@TO_DO
 * */
void *CreateFiber(size_t stack_size, void *(*routine)(void *), void *args) {
    return NULL;
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
    void* fib = ConvertThreadToFiber();
    SwitchToFiber(fib);
    printf("My pid is %d\n", getpid());
    return 0;
}

int main() {
    
    pthread_t t1;
    pthread_t t2;
    
    pthread_create(&t1, NULL, thread_routine, NULL);
    pthread_create(&t2, NULL, thread_routine, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    //ConvertThreadToFiber();

    return 0;
}
