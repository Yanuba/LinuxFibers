#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "Fibers.h"

/******************** 
 *  TEST PROGRAM    *
 ********************/
fiber_t* fib;

void thread_routine2(void* arg) {
    printf("FIIIIIIIIIIIIIBER\n");
    
    double l1 = 96;
    double l2 = 83.9;
    double l3 = l1*l2;
    
    unsigned long x = (unsigned long) arg;
    printf("%lu\n", x);
    printf("%f\n", l3);

    unsigned long idx = FlsAlloc();
    printf("? Got fls index: %ld\n", idx);

    FlsSetValue(idx, 150);

    long long b = FlsGetValue(idx);

    printf("%llu\n", b);

    printf("%d\n", FlsFree(idx));

    printf("FIIIIIIIIIIIIIBER\n");
    
    SwitchToFiber(fib);
    
    printf("FIIIIIIIIIIIIIBER22\n");

    SwitchToFiber(fib);

    //while(1) {printf("%f, %lu\n", l3, x);};

}

void* thread_routine(void* arg) {
    fib = ConvertThreadToFiber();
    //printf("My pid is %d, the fid get is: %d\n", getpid(), *fib);
    fiber_t* fib2 = ConvertThreadToFiber();
    //printf("My pid is %d, the fid get is: %d\n", getpid(), *fib2);
    fib2 = CreateFiber(2*4096, thread_routine2, (void *) (unsigned long) 50);
    //printf("My pid is %d, the fid get is: %d\n", getpid(), *fib2);

    unsigned long idx = FlsAlloc();
    printf("! Got fls index: %ld\n", idx);
    
    SwitchToFiber(fib2);
    printf("My pid is bacl from hell\n");
    SwitchToFiber(fib2);
    printf("My pid is bacl from hello\n");

    printf("! %d\n", FlsFree(idx));
    FlsSetValue(idx, 240);
    long long b = FlsGetValue(idx);
    printf("! %llu\n", b);

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