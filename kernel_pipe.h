#include "tinyos.h"
#include "kernel_cc.h"
#include "kernel_streams.h"

#define PIPE_BUFFER_SIZE 8192	//size of buffer

/** @brief The pipe control block.
 
 * 	A pipe control block takes care of pipe parameters
 *  by controlling them and providing Condition Variables
 *  used for flagging and various checks.
 */

typedef struct pipe_control_block { 
	
	FCB *reader, *writer;

	CondVar has_space; 
	CondVar has_data;

	uint w_position, r_position

	char BUFFER[PIPE_BUFFER_SIZE];

} pipe_cb;


int pipe_read(void* this, char* buffer, unsigned int size);

int pipe_write(void* this, const char* buffer, unsigned int size);

int pipe_close_reader(void* this);

int pipe_close_writer(void* this);

pipe_CB* pipe_init();