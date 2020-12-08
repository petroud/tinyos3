#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_proc.h"

#define PIPE_BUFFER_SIZE 8192	//size of buffer

int pipe_read(void* this, char* buffer, unsigned int size);
int pipe_write(void* this, const char* buffer, unsigned int size);
int pipe_close_reader(void* this);
int pipe_close_writer(void* this);

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

typedef struct pipe_control_block { 
	
	FCB *reader, *writer;

	CondVar has_space; 
	CondVar has_data;

	uint w_position, r_position;

	char BUFFER[PIPE_BUFFER_SIZE];

} pipe_cb;


pipe_cb* pipe_init(){

	pipe_cb* pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	
	assert(pipe);

	pipe->r_position = pipe->w_position = 0;
	pipe->has_data = COND_INIT;
	pipe->has_space = COND_INIT;

	return pipe;	
}


int sys_Pipe(pipe_t* pipe)
{

	FCB *fcbs[2];
	Fid_t fids[2];

	assert(FCB_reserve(2,fids,fcbs));

	pipe_cb* newPipe = pipe_init();
	assert(newPipe);

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

	if(curPipe==NULL || size<=0){
		return -1;
	}


	for(int i=0; i<=size; i++){
		
		buffer[i] = curPipe->BUFFER[curPipe->r_position];

		curPipe->r_position = curPipe->r_position + 1;
	}

	return 4;
}



int pipe_write(void* this, const char* buffer, unsigned int size){
	return -1;
}
int pipe_close_reader(void* this){
	return -1;
}
int pipe_close_writer(void* this){
	return -1;
}