
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "unit_testing.h"
#include "kernel_cc.h"


/*
  Initialize a new PTCB
*/
PTCB* spawn_ptcb(PCB* pcb)
{
	// Allocate a new PTCB 
	PTCB* ptcb = (PTCB*)xmalloc(sizeof(PTCB));

	ptcb->task = NULL;
	ptcb->detached = 0;
	ptcb->exited = 0;
	ptcb->tcb = NULL;
	ptcb->exit_cv = COND_INIT;
	ptcb->argl = 0;
	ptcb->args = NULL;
  ptcb->exitval = 0;
	ptcb->refcount = 1;
	return ptcb;
}

/*
  Increase this PTCB's refcount.
*/
void rcinc(PTCB* ptcb)
{
  ptcb->refcount ++;
}

/*
  Decrease this PTCB's refcount.
*/
void rcdec(PTCB* ptcb)
{
  ptcb->refcount --;

  if(ptcb->refcount == 0)
  {
    free(ptcb);
  }
}

void start_common_thread()
{
  int exitval;

  Task call = CURTHREAD->ptcb->task;
  int argl = CURTHREAD->ptcb->argl;
  void* args = CURTHREAD->ptcb->args;

  exitval = call(argl,args);
  ThreadExit(exitval);
}


/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
  PCB* curproc = CURPROC;

  PTCB* ptcb = spawn_ptcb(CURPROC);

  TCB* new_thread = spawn_thread(curproc, ptcb, start_common_thread);

  ptcb->task = task;
  ptcb->argl = argl;
  ptcb->args = args;

  rlnode_init(& ptcb->ptcb_list_node, NULL);

  rlnode* node = (rlnode*) malloc(sizeof(rlnode));
  rlnode_new(node)->obj = ptcb;
  rlist_push_back(& curproc->ptcb_list, node);

  curproc->thread_count++;

  ASSERT(curproc->thread_count == rlist_len(& curproc->ptcb_list));

  int wakeup_value = wakeup(new_thread);

  ASSERT(wakeup_value == 1);

  ptcb->refcount++;

  return (Tid_t)ptcb;
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
int sys_ThreadJoin(Tid_t tid, int* exitval)
{
	return -1;
}

/**
  @brief Detach the given thread.
  */
int sys_ThreadDetach(Tid_t tid)
{
  Tid_t tidCur = sys_ThreadSelf();
  PTCB* ptcb = (PTCB*)tid;

  if(rlist_find(& CURPROC->ptcb_list,ptcb,NULL) == NULL){
    return -1;
  }

  if(tid == tidCur){
    return 0;
  }
  
  if(tid==NOTHREAD || ptcb->exited == 1){
    return -1;
  }

  ptcb->exited = 1;
  kernel_broadcast(& ptcb->exit_cv);
  ptcb->refcount = 0;

  return 0;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{
  
  /* Acquire the ptcb of the current thread running*/
  PTCB* ptcb = CURTHREAD->ptcb;
  /* Mark the ptcb as exited and set the exit value upon the function parameter */
  ptcb->exited = 1;
  ptcb->exitval = exitval;

  /* Kernel informs all the waiting CVs that a CV is exited and it removes it from the ring*/
  kernel_broadcast(& ptcb->exit_cv);


  /* If the process has no other PTCBs running threads it terminates */
  if (ptcb->tcb->owner_pcb->thread_count == 0)
  {
    sys_Exit(exitval);
  }

  /* Else the thread count of the PCB is reduced by 1 and the node corresponding to the ptcb of the thread terminated is deleted from the list of ptcbs */
  ptcb->tcb->owner_pcb->thread_count--;
  rlist_remove(& ptcb->ptcb_list_node);

  /* Clear the memory address containing previously the ptcb */
  free(ptcb);

  /* Release the kernel for future use */
  kernel_sleep(EXITED, SCHED_USER);
}

