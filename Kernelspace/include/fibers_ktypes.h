#ifndef FKTYPES_H
#define FKTYPES_H

#include <asm/fpu/types.h>

#define FIBER_RUNNING 1
#define FIBER_WAITING 0

#define MAX_FLS_INDEX 1024 //Up to 8Kb for fiber

struct module_hashtable
{
    DECLARE_HASHTABLE(htable, 15);
    spinlock_t lock;
};

struct process_active
{
    spinlock_t lock;
    pid_t tgid;
    pid_t next_fid;                   //will be next fber id
    struct hlist_head running_fibers; //running fibersof
    struct hlist_head waiting_fibers; //waiting fibersof
    struct hlist_node next;           //other process
};

struct fls_struct
{
    unsigned long size;
    long long *fls;
    unsigned long *used_index;
};

struct fiber_struct
{
    pid_t fiber_id;
    pid_t thread_on;        //last thread the fiber runs on
    struct hlist_node next; //must queue fibers
    struct fls_struct fls;  //Fiber local storage

    struct pt_regs regs; //CPU state
    struct fpu fpu_regs; //FPU state

    /* Fields to be exposed in proc */
    char name[12];
    short status;                      //The fiber is running or not?
    void (*entry_point)(void *);       //entry point of the fiber (EIP)
    pid_t parent_process;              //key in the hash table
    pid_t parent_thread;               //pid of the thread that created the fiber
    int activations;                   //the number of current activations of the Fiber
    int failed_activations;            //the number of failed activations
    unsigned long long execution_time; //total execution time in that Fiber context
    unsigned long long last_time;      //CPU time when the fiber was schedulet last time
};

inline struct process_active *find_process(struct module_hashtable *hashtable, pid_t tgid);

#endif /* !FKTYPES_H */