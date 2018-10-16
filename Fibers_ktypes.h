#ifndef FKTYPES_H
#define FKTYPES_H

#include <linux/types.h>
#include <linux/ptrace.h>
#include <linux/hashtable.h>
#include <asm/fpu/types.h>

#include "Fibers_utypes.h"

#define FIBER_RUNNING 1
#define FIBER_WAITING 0

/* 
 * Hash table, each bucket should refer to a process, we anyway check the pid in case of conflicts (but this barely happens)
 * Max pid on this system is 32768, if we exclude pid 0 we can have 32767 different pids,
 * which require 15 bits (which are crearly enough for not having collisions)
 * 
 * Wrapped in a struct so we can pass it as function parameter
 * */
struct module_hashtable{
    DECLARE_HASHTABLE(htable, 15);
};

struct process_active {
    pid_t tgid;                          //process id
    pid_t next_fid;                    //will be next fiber id                        
    struct hlist_head running_fibers;   //running fibers of the process
    struct hlist_head waiting_fibers;   //waiting fibers of the process
    struct hlist_node next;            //other process in the bucket
};

struct fiber_struct {
    pid_t fiber_id;
    pid_t thread_on;        //last thread the fiber runs on
    struct hlist_node next; //must queue fibers
    
    //maybe we need only the pointer?
    struct pt_regs regs; //regs = task_pt_regs(current); (where regs is of type struct pt_regs*)  
    struct fpu fpu_regs; //fpu__save(struct fpu *) and fpu__restore(struct fpu *)? //why not fpu_save with an fpu_state_struct?

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