
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"

/*
  Initialize a new PTCB
*/
PTCB* spawn_ptcb(PCB* pcb, void (*func)())
{
	// Allocate a new PTCB 
	PTCB* ptcb = (PTCB*)xmalloc(sizeof(PTCB));

	ptcb->task = func;
	ptcb->detached = 0;
	ptcb->exited = 0;
	ptcb->tcb = pcb->main_task;
	ptcb->exit_cv = COND_INIT;
	ptcb->argl = 0;
	ptcb->args = NULL;
	ptcb->refcount = 0;

	/*@petroud I changed it from this:
	rlnode_init(& ptcb->ptcb_list_node,NULL);
	to this:
	*/
	rlist_push_back(& pcb->ptcb_list,rlnode_init(& ptcb->ptcb_list_node,ptcb));
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

/** 
  @brief Create a new thread in the current process.
  */
Tid_t sys_CreateThread(Task task, int argl, void* args)
{
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

