#ifndef FKIOCTLS_H
#define FKIOCTLS_H 

#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/hashtable.h>
#include <asm/fpu/types.h>

#define FIBER_RUNNING 1
#define FIBER_WAITING 0

#define MAX_FLS_INDEX 1024 //Up to 8Kb for fiber

#define get_cpu_time() ((current->utime)+(current->stime))

/* 
 * Hash table, each bucket should refer to a process, we anyway check the pid in case of conflicts (but this barely happens)
 * Max pid on this system is 32768, if we exclude pid 0 we can have 32767 different pids,
 * which require 15 bits (which are crearly enough for not having collisions)
 * 
 * Wrapped in a struct so we can pass it as function parameter
 * */
struct module_hashtable
{
    DECLARE_HASHTABLE(htable, 15); //array of hlist_head
    spinlock_t lock;
};

/*
 * Struct to handle the fls
 * We have an array dynamically allocated for fls
 * and we use a bitmask to identify whether an index is available or not.
 * */
struct fls_struct 
{
    //spinlock_t fls_lock;            // spinlock to fls (spin_lock(&lock), spin_unlock(&lock))
    unsigned long   size;           // number of index used
    long long       *fls;           // fls array
    unsigned long   *used_index;    // bitmap for marking fls index allocated
};

/*
 * This will be destroyed only when one of the fibers/thread/(someone) call exit()
 * */
struct process_active 
{
    spinlock_t lock;                    //lock for fiber_id
    pid_t tgid;                         //process id
    pid_t next_fid;                     //will be next fiber id - this is a weakpoint (make it atomic?)                   
    struct hlist_head running_fibers;   //running fibers of the process
    struct hlist_head waiting_fibers;   //waiting fibers of the process
    struct hlist_node next;             //other process in the bucket
};

struct fiber_struct 
{
    pid_t fiber_id;
    pid_t thread_on;        //last thread the fiber runs on
    struct hlist_node next; //must queue fibers
    struct fls_struct fls;  //Fiber local storage

    struct pt_regs regs;    //CPU state
    struct fpu fpu_regs;    //FPU state

    /* Fields to be exposed in proc */
    short               status;                 //The fiber is running or not?
    void                (*entry_point)(void*);  //entry point of the fiber (EIP)
    pid_t               parent_process;         //key in the hash table
    pid_t               parent_thread;          //pid of the thread that created the fiber
    int                 activations;            //the number of current activations of the Fiber
    int                 failed_activations;     //the number of failed activations
    unsigned long long  execution_time;         //total execution time in that Fiber context
    unsigned long long  last_time;              //CPU time when the fiber was schedulet last time
};

long _ioctl_convert(struct process_active *process, fiber_t *arg);
long _ioctl_create(struct process_active *process, struct fiber_args *args);
long _ioctl_switch(struct process_active *process, fiber_t* usr_id_next);

long _ioctl_alloc(struct process_active *process, long* index);
long _ioctl_free(struct process_active *process, long* index);
long _ioctl_get(struct process_active *process, struct fls_args *);
long _ioctl_set(struct process_active *process, struct fls_args *);

int _cleanup(struct module_hashtable*);

#endif /* !FKIOCTLS_H */