
#include <assert.h>
#include "kernel_cc.h"
#include "kernel_proc.h"
#include "kernel_streams.h"
#include "unit_testing.h"

/* 
 The process table and related system calls:
 - Exec
 - Exit
 - WaitPid
 - GetPid
 - GetPPid

 */

/* The process table */
PCB PT[MAX_PROC];
unsigned int process_count;

PCB* get_pcb(Pid_t pid)
{
  return PT[pid].pstate==FREE ? NULL : &PT[pid];
}

Pid_t get_pid(PCB* pcb)
{
  return pcb==NULL ? NOPROC : pcb-PT;
}

/* Initialize a PCB */
static inline void initialize_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->argl = 0;
  pcb->args = NULL;
  pcb->thread_count = 0;

  for(int i=0;i<MAX_FILEID;i++)
    pcb->FIDT[i] = NULL;

  rlnode_init(& pcb->children_list, NULL);
  rlnode_init(& pcb->exited_list, NULL);
  rlnode_init(& pcb->children_node, pcb);
  rlnode_init(& pcb->exited_node, pcb);
  pcb->child_exit = COND_INIT;
  rlnode_init(& pcb->ptcb_list, NULL);
}


static PCB* pcb_freelist;

void initialize_processes()
{
  /* initialize the PCBs */
  for(Pid_t p=0; p<MAX_PROC; p++) {
    initialize_PCB(&PT[p]);
  }

  /* use the parent field to build a free list */
  PCB* pcbiter;
  pcb_freelist = NULL;
  for(pcbiter = PT+MAX_PROC; pcbiter!=PT; ) {
    --pcbiter;
    pcbiter->parent = pcb_freelist;
    pcb_freelist = pcbiter;
  }

  process_count = 0;

  /* Execute a null "idle" process */
  if(Exec(NULL,0,NULL)!=0)
    FATAL("The scheduler process does not have pid==0");
}


/*
  Must be called with kernel_mutex held
*/
PCB* acquire_PCB()
{
  PCB* pcb = NULL;

  if(pcb_freelist != NULL) {
    pcb = pcb_freelist;
    pcb->pstate = ALIVE;
    pcb_freelist = pcb_freelist->parent;
    process_count++;
  }

  return pcb;
}

/*
  Must be called with kernel_mutex held
*/
void release_PCB(PCB* pcb)
{
  pcb->pstate = FREE;
  pcb->parent = pcb_freelist;
  pcb_freelist = pcb;
  process_count--;
}


/*
 *
 * Process creation
 *
 */




/*
	This function is provided as an argument to spawn,
	to execute the main thread of a process.
*/
void start_main_thread()
{
  int exitval;

  Task call =  CURPROC->main_task;
  int argl = CURPROC->argl;
  void* args = CURPROC->args;

  exitval = call(argl,args);
  Exit(exitval);
}


/*
	System call to create a new process.
 */
Pid_t sys_Exec(Task call, int argl, void* args)
{
  PCB *curproc, *newproc;
  
  /* The new process PCB */
  newproc = acquire_PCB();

  if(newproc == NULL) goto finish;  /* We have run out of PIDs! */

  if(get_pid(newproc)<=1) {
    /* Processes with pid<=1 (the scheduler and the init process) 
       are parentless and are treated specially. */
    newproc->parent = NULL;
  }
  else
  {
    /* Inherit parent */
    curproc = CURPROC;

    /* Add new process to the parent's child list */
    newproc->parent = curproc;
    rlist_push_front(& curproc->children_list, & newproc->children_node);

    /* Inherit file streams from parent */
    for(int i=0; i<MAX_FILEID; i++) {
       newproc->FIDT[i] = curproc->FIDT[i];
       if(newproc->FIDT[i])
          FCB_incref(newproc->FIDT[i]);
    }
  }


  /* Set the main thread's function */
  newproc->main_task = call;

  /* Copy the arguments to new storage, owned by the new process */
  newproc->argl = argl;
  if(args!=NULL) {
    newproc->args = malloc(argl);
    memcpy(newproc->args, args, argl);
  }
  else
    newproc->args=NULL;

  /* 
    Create and wake up the thread for the main function. This must be the last thing
    we do, because once we wakeup the new thread it may run! so we need to have finished
    the initialization of the PCB.
   */
  if(call != NULL) {
    PTCB* ptcb = spawn_ptcb(newproc);

    TCB* main_thread = spawn_thread(newproc, ptcb, start_main_thread);
    
    ptcb->task = call;
    ptcb->argl = argl;
    ptcb->args = args;

    rlnode_init(& ptcb->ptcb_list_node, ptcb);
    rlnode_init(& newproc->ptcb_list, NULL);

    rlist_push_back(& newproc->ptcb_list, &ptcb->ptcb_list_node);

    newproc->thread_count++;
    newproc->main_thread = main_thread;
    
    wakeup(newproc->main_thread);

  }


finish:
  return get_pid(newproc);
}


/* System call */
Pid_t sys_GetPid()
{
  return get_pid(CURPROC);
}


Pid_t sys_GetPPid()
{
  return get_pid(CURPROC->parent);
}


static void cleanup_zombie(PCB* pcb, int* status)
{
  if(status != NULL)
    *status = pcb->exitval;

  rlist_remove(& pcb->children_node);
  rlist_remove(& pcb->exited_node);

  release_PCB(pcb);
}


static Pid_t wait_for_specific_child(Pid_t cpid, int* status)
{

  /* Legality checks */
  if((cpid<0) || (cpid>=MAX_PROC)) {
    cpid = NOPROC;
    goto finish;
  }

  PCB* parent = CURPROC;
  PCB* child = get_pcb(cpid);
  if( child == NULL || child->parent != parent)
  {
    cpid = NOPROC;
    goto finish;
  }

  /* Ok, child is a legal child of mine. Wait for it to exit. */
  while(child->pstate == ALIVE)
    kernel_wait(& parent->child_exit, SCHED_USER);
  
  cleanup_zombie(child, status);
  
finish:
  return cpid;
}


static Pid_t wait_for_any_child(int* status)
{
  Pid_t cpid;

  PCB* parent = CURPROC;

  /* Make sure I have children! */
  if(is_rlist_empty(& parent->children_list)) {
    cpid = NOPROC;
    goto finish;
  }

  while(is_rlist_empty(& parent->exited_list)) {
    kernel_wait(& parent->child_exit, SCHED_USER);
  }

  PCB* child = parent->exited_list.next->pcb;
  assert(child->pstate == ZOMBIE);
  cpid = get_pid(child);
  cleanup_zombie(child, status);

finish:
  return cpid;
}


Pid_t sys_WaitChild(Pid_t cpid, int* status)
{
  /* Wait for specific child. */
  if(cpid != NOPROC) {
    return wait_for_specific_child(cpid, status);
  }
  /* Wait for any child */
  else {
    return wait_for_any_child(status);
  }

}


void sys_Exit(int exitval)
{
  /* Right here, we must check that we are not the boot task. If we are, 
     we must wait until all processes exit. */
  if(sys_GetPid()==1) {
    while(sys_WaitChild(NOPROC,NULL)!=NOPROC);
  }

  PCB *curproc = CURPROC;  /* cache for efficiency */
  curproc->exitval = exitval;

  sys_ThreadExit(exitval);
}

typedef struct info_control_block {
	uint iterator;
  procinfo curinfo;
} info_cb;

//Construct the stream containing the info for process 
//It kinda reads it...
int sysinfo_read(void* argvoid, char* buff, unsigned int size) {

  info_cb* icb = (info_cb*) argvoid;

  Pid_t current_pid = -1;
  
  //iterate through the table of the proccesses by using the iterator
  while (icb->iterator<MAX_PROC) {
    
    //if it finds a table cell that isn't free, keeps the pid of it because its a process
    if(&PT[icb->iterator].pstate != FREE){
      current_pid = icb->iterator;
      
      icb->iterator++;
      break;
    }
    //keeps looking for some processes in the next call 
    icb->iterator++;
  }
 
  PCB* curPCB = get_pcb(current_pid);

  //it may happen...
  if(curPCB == NULL){
    return -1;
  }

  //takes the info of the process and places it in to the icb procinfo block
  icb->curinfo.pid = get_pid(curPCB);
  icb->curinfo.ppid = get_pid(curPCB->parent);  
  
  icb->curinfo.alive = curPCB->pstate == ALIVE ? 1 : 0;

  icb->curinfo.main_task = curPCB->main_task;
  icb->curinfo.argl = curPCB->argl;
  icb->curinfo.thread_count = curPCB->thread_count;
  
  //copy the arguments of the process to the icb procinfo block
  memcpy(&icb->curinfo.args, curPCB->args, (curPCB->argl <= PROCINFO_MAX_ARGS_SIZE ? curPCB->argl : PROCINFO_MAX_ARGS_SIZE));

  //copy the whole stream to the buffer argument
  memcpy(buff, (char*)&icb->curinfo, size);
  return size;
}

//finds the icb variable
//returns 0 if the sysinfo_close close the icb
//returns -1 if the variable was allready null
//Then frees the allocated space for it
int sysinfo_close(void *argvoid) {
	info_cb* icb= (info_cb*)argvoid;

  if(icb!=NULL){
    icb = NULL;
    free(icb);
    return 0;
  }

  return -1;
}

file_ops info_operations= {
		.Open = NULL,
		.Read = sysinfo_read,
		.Write = NULL,
		.Close = sysinfo_close
};

//Finds space for the variable of the info control block,
//assigns some parameters to the object and the functions of the stream
Fid_t sys_OpenInfo(){

  Fid_t fid[1];
  FCB* fcb[1];

  if(!FCB_reserve(1,fid,fcb)){
    return NOFILE;
  }

  info_cb* icb = (info_cb*)xmalloc(sizeof(info_cb));

  icb->iterator = 1;
  
  fcb[0]->streamobj = icb;
  fcb[0]->streamfunc = &info_operations;

  return fid[0];
}
