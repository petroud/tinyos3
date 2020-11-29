
#include "tinyos.h"
#include "kernel_pipe.h"
#include "kernel_streams.h"


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


pipe_cb* spawn_pipe(){

	pipe_cb* pipe = (pipe_cb*)xmalloc(sizeof(pipe_cb));
	
	assert(pipe);

	pipe->r_position = pipe->w_position = 0;
	pipe->has_data = COND_INIT;
	pipe->has_space = COND_INIT;

	return pipe;	
}


int sys_Pipe(pipe_t* pipe)
{
	return -1;
}

