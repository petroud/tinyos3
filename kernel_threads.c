
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "unit_testing.h"

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
	ptcb->refcount = 1;

	pcb->thread_count ++;
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
  PCB* pcb = CURPROC;
  PTCB* ptcb = spawn_ptcb(pcb);
  
  TCB* common_thread = spawn_thread(pcb,ptcb,start_common_thread);

  ptcb->argl = argl;
  ptcb->args = args;
  ptcb->task = task;

  rlnode_init(& ptcb->ptcb_list_node, NULL);
  rlnode* node = (rlnode*) malloc(sizeof(rlnode));
  rlnode_new(node)->obj = ptcb;
  rlist_push_back(& pcb->ptcb_list, node);
  
  if(task!=NULL){
    ASSERT(pcb->thread_count == rlist_len(& pcb->ptcb_list));
    int wakeup_value = wakeup(common_thread);
    ASSERT(wakeup_value == 1);

    pcb->thread_count++;
    return (Tid_t)(common_thread->ptcb);
  }

	return NOTHREAD;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t sys_ThreadSelf()
{
	return (Tid_t) CURTHREAD;
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
	return -1;
}

/**
  @brief Terminate the current thread.
  */
void sys_ThreadExit(int exitval)
{

}

