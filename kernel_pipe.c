#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_sched.h"


int pipe_read(void* this, char* buffer, unsigned int size);
int pipe_write(void* this, const char* buffer, unsigned int size);
int pipe_close_reader(void* this);
int pipe_close_writer(void* this);


//Opeartion sets for pipes 

//Read operation set
static file_ops readOperations = {
	.Open = NULL,
	.Read = pipe_read,
	.Write = NULL,
	.Close = pipe_close_reader
};

//Write operation set
static file_ops writeOperations = {
	.Open = NULL,
	.Read = NULL,
	.Write = pipe_write,
	.Close = pipe_close_writer
};


//Allocates a pipe and initializes some of each values
pipe_cb* pipe_init(){

	pipe_cb* pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	
	pipe->r_position = pipe->w_position = 0;
	pipe->has_data = COND_INIT;
	pipe->has_space = COND_INIT;

	//Capacity variable counts how many bytes we are capable of storing the pipe buffer
	pipe->capacity = PIPE_BUFFER_SIZE-1;

	return pipe;	
}

//Allocates the pointers in each side of the pipe
//Then connects the stream functions to the operations
int sys_Pipe(pipe_t* pipe)
{

	FCB *fcbs[2];
	Fid_t fids[2];

	//reserves an 2 FCBs to connect them to the pipe read and write end
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


//Reads from a pipe
int pipe_read(void* this, char* buffer, unsigned int size){
	
	//finds the pipe from casting *this object
	pipe_cb* curPipe = (pipe_cb*)this;

	//important error check, otherwise it cannot read the pipe
	if(curPipe==NULL || size<=0 || curPipe->reader == NULL){
		return -1;
	}

	//waits for the writers to write to the pipe, then reads it
	while(curPipe->writer!=NULL && curPipe->capacity == PIPE_BUFFER_SIZE-1){
		kernel_broadcast(&curPipe->has_space);
		kernel_wait(&curPipe->has_data,SCHED_PIPE);
	}

	int i = 0;
	for(i=0; i<size; i++){

		//in case the writer is closed, and all bytes have been read the readers returns
	 	
		 if(curPipe->writer==NULL && curPipe->capacity == PIPE_BUFFER_SIZE-1){
			return i;
		}

		//Move the pointer to the buffer to read
		buffer[i] = curPipe->BUFFER[curPipe->r_position];

		curPipe->r_position = (curPipe->r_position + 1);

		if(curPipe->r_position==PIPE_BUFFER_SIZE){
			curPipe->r_position = 0;
		}

		curPipe->capacity++;
	}

	//the reading is finished and it informs the writers to write
	kernel_broadcast(&curPipe->has_space);
	return i;
}


//Writes to the pipe
int pipe_write(void* this, const char* buffer, unsigned int size){

	pipe_cb* curPipe = (pipe_cb*)this;
	
	//important error check, else it cannot write to the pipe
	if(curPipe==NULL || size<=0 || curPipe->writer == NULL || curPipe->reader == NULL){
		return -1;
	}

	//tells the reader that it has written, waits for the reader to read and empty the buffer
	while(curPipe->reader!=NULL && curPipe->capacity == 0){
			kernel_broadcast(&curPipe->has_data);
			kernel_wait(&curPipe->has_space,SCHED_PIPE);
	}

	int i = 0;
	for(i=0; i<size; i++){
		
		//if reader does not exsist, the writer has no job left
		if(curPipe->writer == NULL || curPipe->reader == NULL){
			return -1;
		}	
		
	 	if(curPipe->reader==NULL && curPipe->capacity == 0){
			return -1;
		}

		if(curPipe->reader==NULL){

			return -1;
		}

		//Move the pointer to the buffer to write
		curPipe->BUFFER[curPipe->w_position] = buffer[i];

		curPipe->w_position = (curPipe->w_position+1);

		if(curPipe->w_position == PIPE_BUFFER_SIZE){	
			curPipe->w_position = 0;
		}

		//update capacity variable
		curPipe->capacity--;
	}

	//kernel_broadcast(&curPipe->has_data);
	return i;
}



//Closes the reader
int pipe_close_reader(void* this){
	
	pipe_cb* curPipe = (pipe_cb*)this;

	if(curPipe==NULL || curPipe->reader == NULL){
		return -1;
	}

	//closes the reader, making him point to a null location
	curPipe->reader=NULL;
	//frees the space the pipe was in, if the writer does not exist too
	if(curPipe->writer == NULL){
		free(curPipe);
	}
	return 0;
}

//Closes the writer
int pipe_close_writer(void* this){

	pipe_cb* curPipe = (pipe_cb*)this;
	if(curPipe==NULL || curPipe->writer == NULL){
		return -1;
	}


	//closes the writer, making him point to a null location
	curPipe->writer=NULL;
	//call the readers to read it possible data left in the buffer
	if(curPipe->reader!=NULL){
		kernel_broadcast(&curPipe->has_data);
	}
	
	//Then frees the space the pipe was in
	if(curPipe->reader==NULL){
		free(curPipe);
	}

	return 0;
}