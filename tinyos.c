#include <stdlib.h>
#include <assert.h>
#include <stdio_ext.h>


#include "tinyos.h"
#include "tinyoslib.h"
#include "kernel_proc.h"
#include "util.h"


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
    }else{
        /* code */
    }
}


/**
 * This function terminates the current thread running.
 * @arg int, the exit value of the thread
 */
void ThreadExit(int exitval){
    
}


