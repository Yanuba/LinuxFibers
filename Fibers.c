#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stropts.h>

#include <pthread.h>

#include "FibersLKM.h"
#include "Fibers.h"

void *ConvertThreadToFiber() {
    int ret;
    int fd = open("/dev/FibersLKM", O_RDONLY);
    if (fd == -1) {
        perror("Error opening special device file");
        return NULL;
    }

    struct fiber_struct *ctx = (struct fiber_struct*) malloc(sizeof(struct fiber_struct)); 

    ret = ioctl(fd, IOCTL_CONVERT, ctx);
    
    //check ret value
    
    close(fd);
    return ctx;
}

void *CreateFiber(size_t stack_size, void (*routine)(void *), void *args) {
    return NULL;
}
void SwitchToFiber(void* fiber) {
    return;
}

long FlsAlloc(void) {
    return -1;
}
void FlsFree(long index) {
    return;
}
void *FlsGetValue(long index){
    return NULL;
}
void FlsSetValue(long index, void* value){
    return;
}

void * thread_routine(void* arg) {
    ConvertThreadToFiber();
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
