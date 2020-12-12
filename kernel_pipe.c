#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_sched.h"

static file_ops readOperations = {
	.Open = NULL,
	.Read = pipe_read,
	.Write = NULL,
	.Close = pipe_close_reader
};

static file_ops writeOperations = {
	.Open = NULL,
	.Read = NULL,
	.Write = pipe_write,
	.Close = pipe_close_writer
};


pipe_cb* pipe_init(){

	pipe_cb* pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	
	pipe->r_position = pipe->w_position = 0;
	pipe->has_data = COND_INIT;
	pipe->has_space = COND_INIT;

	pipe->elements = 0;

	return pipe;	
}


int sys_Pipe(pipe_t* pipe)
{

	FCB** fcbs = xmalloc(2*sizeof(FCB*));
	Fid_t* fids = xmalloc(2*sizeof(FCB*));

	int retval = FCB_reserve(2,fids,fcbs);

    if(retval == 0){
		return -1;
	}

	pipe_cb* newPipe = pipe_init();
	if(newPipe==NULL){
		return -1;
	}

	newPipe->reader = fcbs[0];
	newPipe->writer = fcbs[1];

	pipe->read = fids[0];
	pipe->write = fids[1];

	fcbs[0]->streamobj = newPipe;
	fcbs[1]->streamobj = newPipe;
	
	fcbs[0]->streamfunc = &readOperations;
	fcbs[1]->streamfunc = &writeOperations;

	return 0;
}


int pipe_read(void* this, char* buffer, unsigned int size){
	

	pipe_cb* curPipe = (pipe_cb*)this;

	if(curPipe==NULL){
		return -1;
	}

	if(curPipe->reader == NULL){
		return -1;
	}

	/*
	//EOF
	if(curPipe->writer==NULL && curPipe->r_position == curPipe->w_position){
		return 0;
	}*/

	int i = 0;
	for(i=0; i<size; i++){

		while(curPipe->writer!=NULL && curPipe->elements == 0){
			kernel_broadcast(&curPipe->has_space);
			kernel_wait(&curPipe->has_data,SCHED_PIPE);
		}

		//we are done, lets return
	 	if(curPipe->writer==NULL && curPipe->elements == 0){
			return i;
		}
	
		buffer[i] = curPipe->BUFFER[curPipe->r_position];
		curPipe->r_position = (curPipe->r_position + 1)%PIPE_BUFFER_SIZE;
		curPipe->elements--;
	}

	kernel_broadcast(&curPipe->has_space);
	return i;
}




int pipe_write(void* this, const char* buffer, unsigned int size){

	pipe_cb* curPipe = (pipe_cb*)this;
	

	if (curPipe == NULL) {
		return -1;
	}

	if (curPipe->writer == NULL) {
		return -1;
	}

	if (curPipe->reader == NULL) {
		return -1;
	}


	int i = 0;
	for(i=0; i<size; i++){
	
		while(curPipe->reader!=NULL && curPipe->elements == PIPE_BUFFER_SIZE){
			kernel_broadcast(&curPipe->has_data);
			kernel_wait(&curPipe->has_space,SCHED_PIPE);
		}

	 	if(curPipe->reader==NULL){
			return i;
		}
		
		curPipe->BUFFER[curPipe->w_position] = buffer[i];
		curPipe->w_position = (curPipe->w_position+1)%PIPE_BUFFER_SIZE;
		curPipe->elements++;
	}
	
	kernel_broadcast(&curPipe->has_data);
	return i;
}




int pipe_close_reader(void* this){

	pipe_cb* curPipe = (pipe_cb*)this;

	if(curPipe==NULL || curPipe->reader == NULL){
		return -1;
	}
	
	curPipe->reader=NULL;

	return 0;
}


int pipe_close_writer(void* this){

	pipe_cb* curPipe = (pipe_cb*)this;
	if(curPipe==NULL || curPipe->writer == NULL){
		return -1;
	}

	curPipe->writer=NULL;
	if(curPipe->reader!=NULL){
		kernel_broadcast(&curPipe->has_data);
	}

	return 0;
}