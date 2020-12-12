
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "unit_testing.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

/*
  Initialize a new PTCB
*/
PTCB* spawn_ptcb(PCB* pcb)
{
	// Allocate a new PTCB 
	PTCB* ptcb = (PTCB*)malloc(sizeof(PTCB));
  ASSERT (ptcb);

	ptcb->task = NULL;
	ptcb->detached = 0;
	ptcb->exited = 0;
	ptcb->tcb = NULL;
	ptcb->exit_cv = COND_INIT;
	ptcb->argl = 0;
	ptcb->args = NULL;
  ptcb->exitval = 0;
	ptcb->refcount = 1;

  rlnode_init(& ptcb->ptcb_list_node, NULL);

	return ptcb;
}

/**
 *
	This function is provided as an argument to spawn_thread,
	to handle the execution and the exit procedure of the thread
  of a PTCB. 
 */
void start_common_thread()
{
  int exitval;

  Task call = CURTHREAD->ptcb->task;
  int argl = CURTHREAD->ptcb->argl;
  void* args = CURTHREAD->ptcb->args;

  exitval = call(argl,args);
  sys_ThreadExit(exitval);
}


/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  //The thread will be created in the current calling-running process
  PCB* curproc = CURPROC;

  //Initialize a new PTCB
  PTCB* ptcb = spawn_ptcb(CURPROC);

  //Store the task and the parameters of it in the ptcb so they can be
  //accessible from the thread later
  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  //Initialize the PTCB node to connect it with itself and insert it 
  //in the list of the PTCBs of the process
  rlist_push_back(& curproc->ptcb_list, rlnode_init(&ptcb->ptcb_list_node, ptcb));


  //The process has a new thread!
  curproc->thread_count++;

  if(task!=NULL){
    //Create a TCB and initialize it with according parameters and show it the way it will be started and exited
    TCB* new_thread = spawn_thread(curproc, ptcb, start_common_thread);
    //Make the thread READY
    wakeup(new_thread);
    return (Tid_t)ptcb;
  }

  //Usually we dont get here
  return -1;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD->ptcb;
}

/**
  @brief Join the given thread.
  */
int sys_ThreadJoin(Tid_t tid, int* exitval){

  //What's the Tid_t of the current thread?
  Tid_t tidCur = sys_ThreadSelf();

  //What's the PTCB corresponding to the Tid_t given as argument
  PTCB* ptcb = (PTCB*)tid;

  //Check if the PTCB exists in the PTCB list of the current process
  if(rlist_find(& CURPROC->ptcb_list,ptcb,NULL) == NULL){
    return -1;
  }

  //If the Tid_t is invalid or if the thread tries to join itself return error
  if(tid==NOTHREAD || tid == tidCur){
    return -1;
  }

  //The kernel wait will use a reference to the PTCB so we increase the counter
  ptcb->refcount++;

  //While the joined thread isn't exited or detached all the joiners are waiting through kernel_wait function
  //the Cond_Var to be in exited state
  while(ptcb->exited==0 && ptcb->detached==0){
    kernel_wait(&ptcb->exit_cv, SCHED_USER);
  }

  //The exitval might be NULL we don't want to do a NULL assignment
  if(exitval!=NULL){
    *exitval = ptcb->exitval;  
  }

  //If the joined thread had became detached after joiners are attached to it an error occurs 
  if(ptcb->detached==1){
    return -1;
  }

  ptcb->refcount--;

  //Joining succeed
  return 0;
}



/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  //What's the Tid_t of the current thread?
  //Tid_t tidCur = sys_ThreadSelf();

  //What's the PTCB corresponding to the Tid_t given as argument
  PTCB* ptcb = (PTCB*)tid;

  //Check if the PTCB exists in the PTCB list of the current process
  if(rlist_find(& CURPROC->ptcb_list,ptcb,NULL) == NULL){
    //If it does not exist in the list or the find function returns NULL then error returns
    return -1;
  }
  
  //Check if the tid given is ZERO or if the corresponding PTCB is exited
  if(tid==NOTHREAD) return -1;
  
  if(ptcb->exited == 1) return -1;

  //If everything is normal detach the thread which was given as argument
  ptcb->detached=1;
  kernel_broadcast(& ptcb->exit_cv);
  //Reset the references coutner of the PTCB to initial value
  ptcb->refcount=1;

  return 0;
}



/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  PTCB* ptcb = (PTCB*)sys_ThreadSelf();

  /* Mark the ptcb as exited and set the exit value upon the function parameter */
  ptcb->exited = 1;
  ptcb->exitval = exitval;

  /* Else the thread count of the PCB is reduced by 1 and the node corresponding to the ptcb of the thread terminated is deleted from the list of ptcbs */
  ptcb->tcb->owner_pcb->thread_count--;

  PCB* curproc = CURPROC;

  //Only if the last thread of the PCB is exiting, the process will be terminated too
  if(CURPROC->thread_count == 0){
      /* Do all the other cleanup we want here, close files etc. */
    if(curproc->args) {
      free(curproc->args);
      curproc->args = NULL;
    }

    /* Clean up FIDT */
    for(int i=0;i<MAX_FILEID;i++) {
      if(curproc->FIDT[i] != NULL) {
       FCB_decref(curproc->FIDT[i]);
        curproc->FIDT[i] = NULL;
      }
    }

    /* Reparent any children of the exiting process to the 
       initial task */
    PCB* initpcb = get_pcb(1);
    while(!is_rlist_empty(& curproc->children_list)) {
     rlnode* child = rlist_pop_front(& curproc->children_list);
     child->pcb->parent = initpcb;
     rlist_push_front(& initpcb->children_list, child);
    }

    /* Add exited children to the initial task's exited list 
       and signal the initial task */
    if(!is_rlist_empty(& curproc->exited_list)) {
     rlist_append(& initpcb->exited_list, &curproc->exited_list);
      kernel_broadcast(& initpcb->child_exit);
    }

    /* Put me into my parent's exited list */
    if(curproc->parent != NULL) {   /* Maybe this is init */
     rlist_push_front(& curproc->parent->exited_list, &curproc->exited_node);
      kernel_broadcast(& curproc->parent->child_exit);
    }

    /* Disconnect my main_thread */
    curproc->main_thread = NULL;

    /* Now, mark the process as exited. */
   curproc->pstate = ZOMBIE;
  }

  kernel_broadcast(& ptcb->exit_cv);
  /* Release the kernel for future use */
  kernel_sleep(EXITED, SCHED_USER);
}

