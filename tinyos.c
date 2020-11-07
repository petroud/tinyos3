#include <stdlib.h>
#include <assert.h>
#include <stdio_ext.h>


#include "tinyos.h"
#include "tinyoslib.h"
#include "kernel_proc.h"
#include "util.h"

#define ZERO 0

static int thread_id_seq = ZERO;

/**
 * This function starts the execution of a process
 * by calling the system function sys_exec and passing
 * the task and its arguments to it.
 */
Pid_t Exec(Task task, int argl, void* args){
    Pid_t new = sys_exec(task, argl, args);
    return new;    
}

/**
 * When this function is called by a process thread, the process terminates
  and sets its exit code to val. 
 */
void Exit(int val){
    
}

/**
 * This function returns the PID of the current process running
 */
Pid_t GetPid(void){
    return get_pid(CURPROC);
}

/**
 * This function returns the PID of the caller's parent
 */
Pid_t GetPPid(void){
    return get_pid(CURPROC->parent);
}


/**
 * Create a new thread in the current process
 * 
 * The new thread is executed in the same process as the calling thread.
 * If this thread returns from funciton task, the return value is used as
 * an argument to 'ThreadExit'.
 */
Tid_t CreateThread(Task task, int argl, void* args){
    
    //First we get the process of the thread currently running
    int curThreadProcess = get_pid(CURTHREAD->owner_pcb);

    //Checking if the PID returned is invalid
    if (curThreadProcess==NULL){
         FATAL("Illegal current process PID occured, when attempting new thread creation");
         return ZERO;
    }else{
        TCB* tcb = spawn_thread(get_pcb(curThreadProcess), task);
        Tid_t tid = (uintptr_t)thread_id_seq++;
        return tid;
    }
}


/**
 * This function terminates the current thread running.
 * @arg int, the exit value of the thread
 */
void ThreadExit(int exitval){

       CURCORE.current_thread->state = EXITED;
}


