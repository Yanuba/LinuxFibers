#ifndef FKTYPES_H
#define FKTYPES_H

#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/hashtable.h>
#include <asm/fpu/types.h>

#include "Fibers_utypes.h"

#define FIBER_RUNNING 1
#define FIBER_WAITING 0

#define MAX_FLS_INDEX 4096 //Up to 32Kb for process

/* 
 * Hash table, each bucket should refer to a process, we anyway check the pid in case of conflicts (but this barely happens)
 * Max pid on this system is 32768, if we exclude pid 0 we can have 32767 different pids,
 * which require 15 bits (which are crearly enough for not having collisions)
 * 
 * Wrapped in a struct so we can pass it as function parameter
 * */
struct module_hashtable
{
    DECLARE_HASHTABLE(htable, 15);
};

/*
 * Struct to handle the fls
 * We have an array dynamically allocated for fls
 * and we use a bitmask to identify whether an index is available or not.
 * */
struct fls_struct 
{
    spinlock_t fls_lock;            // spinlock to fls (spin_lock(&lock), spin_unlock(&lock))
    unsigned long   size;           // number of index used
    long long       *fls;           // fls array
    unsigned long   *used_index;    // bitmap for marking fls index allocated
};

struct process_active 
{
    pid_t tgid;                         //process id
    pid_t next_fid;                     //will be next fiber id                        
    struct hlist_head running_fibers;   //running fibers of the process
    struct hlist_head waiting_fibers;   //waiting fibers of the process
    struct hlist_node next;             //other process in the bucket
    struct fls_struct *fls;             //Fiber local storage
};

struct fiber_struct 
{
    pid_t fiber_id;
    pid_t thread_on;        //last thread the fiber runs on
    struct hlist_node next; //must queue fibers
    
    struct pt_regs regs;    //CPU state
    struct fpu fpu_regs;    //FPU state

    /* Fields to be exposed in proc */
    short   status;                 //The fiber is running or not?
    void*   (*entry_point)(void*);  //entry point of the fiber (EIP)
    pid_t   parent_process;         //key in the hash table
    pid_t   parent_thread;          //pid of the thread that created the fiber
    int     activations;            //the number of current activations of the Fiber
    int     failed_activations;     //the number of failed activations
    long    execution_time;         //total execution time in that Fiber context
};

#endif /* !FKTYPES */