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

    fiber_id *id = (fiber_id *) malloc(sizeof(fiber_id)); 

    ret = ioctl(fd, IOCTL_CONVERT, id);
    
    //check ret value    
    //probe do_exit to perform cleanup operations

    close(fd);
    return id;
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
    fiber_id* fib = ConvertThreadToFiber();
    printf("My pid is %d, the fid get is: %d\n", getpid(), *fib);
    return 0;
}

int main() {
    
    pthread_t t1;
    pthread_t t2;
    pthread_t t3;
    
    pthread_create(&t1, NULL, thread_routine, NULL);
    pthread_create(&t2, NULL, thread_routine, NULL);
    pthread_create(&t3, NULL, thread_routine, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    //ConvertThreadToFiber();

    return 0;
}
