
#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"


socket_cb* PORT_MAP[MAX_PORT+1];

Fid_t sys_Socket(port_t port)
{
	if(port > MAX_PORT || port < NOPORT){
		return NOFILE;
	}

	FCB* fcbs[1];
	Fid_t fids[1];

	int retval = FCB_reserve(1,fids,fcbs);

	if(retval == 0){
		return -1;
	}


    socket_cb* socket = xmalloc(sizeof(socket_cb));
	socket->type = SOCKET_UNBOUND;
	socket->port = port;
	socket->refcount = 0;

	socket->fcb = fcbs[0];

	fcbs[0]->streamobj = socket;
	fcbs[0]->streamfunc = &socket_operations;

	return fids[0];
}



int sys_Listen(Fid_t sock)
{

	FCB* fcb = get_fcb(sock);

	
	if(fcb==NULL){
		return -1;
	}

	socket_cb* socket = (socket_cb*)fcb->streamobj;

	//Null or already in use
	if(socket==NULL){
		return -1;
	}

	if(socket->type != SOCKET_UNBOUND){
		return -1;
	}

	if(socket->port == NOPORT){
		return -1;
	}

	if(PORT_MAP[socket->port]!=NULL  ){
		return -1;
	}

	socket->type = SOCKET_LISTENER;

	rlnode_init(&socket->listener_s.queue, NULL);
	socket->listener_s.req_available = COND_INIT;

	PORT_MAP[socket->port] = socket;

	return 0;
}


Fid_t sys_Accept(Fid_t lsock)
{

	FCB* fcb = get_fcb(lsock);

	if(fcb == NULL){
		return -1;
	}

	socket_cb* socket = fcb->streamobj;


	if(socket == NULL ){
		return -1;
	}

	if(socket->type != SOCKET_LISTENER){
		return -1;
	}

	socket->refcount++;


	while((rlist_len(&socket->listener_s.queue) == 0)){
		kernel_wait(&socket->listener_s.req_available, SCHED_PIPE);
	}

	if(PORT_MAP[socket->port]==NULL){
		return -1;
	}

	rlnode* connectionNode = rlist_pop_front(&socket->listener_s.queue);
	connection_request* cr = (connection_request*)connectionNode->obj;
	cr->admitted=1;
	
	Fid_t fid_target = sys_Socket(socket->port);
	FCB* fcb_target = get_fcb(fid_target);
	socket_cb* socket_target = fcb_target->streamobj;

	if(socket_target==NULL){
		return -1;
	}

	socket_target->type = SOCKET_PEER;
	cr->peer->type = SOCKET_PEER;

	socket_target->peer_s.peer = cr->peer;
	cr->peer->peer_s.peer = socket_target;

	pipe_cb* pipe1 = pipe_init();
	pipe_cb* pipe2 = pipe_init();

	cr->peer->peer_s.write_pipe = pipe1;
	cr->peer->peer_s.read_pipe = pipe2;	

	socket_target->peer_s.write_pipe = pipe2;
	socket_target->peer_s.read_pipe = pipe1;

	cr->peer->peer_s.peer = cr->peer;
	socket_target->peer_s.peer = cr->peer;

	kernel_broadcast(&cr->connected_cv);
	socket->refcount--;

	return fid_target;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	FCB* fcb = get_fcb(sock);

	socket_cb* socket = fcb->streamobj;
	socket_cb* socket_target = PORT_MAP[port];


	if(port<NOPORT || port>MAX_PORT){
		return -1;
	}

	if(sock==NOFILE || socket == NULL || socket->type != SOCKET_UNBOUND){
		return -1;
	}

	if(PORT_MAP[socket->port]!=NULL){
		return -1;
	}

	if(socket_target->type!= SOCKET_LISTENER){
		return -1;
	}


	socket->refcount++;

	connection_request* cr = xmalloc(sizeof(connection_request));
	cr->admitted = 0;
	rlnode_init(&cr->queue_node,cr);
	cr->peer = socket_target;
	cr->connected_cv = COND_INIT;

	socket_cb* socket_listens = PORT_MAP[port];
	rlist_push_front(&socket_listens->listener_s.queue, &cr->queue_node);

	kernel_broadcast(&socket_listens->listener_s.req_available);
	kernel_timedwait(&cr->connected_cv,SCHED_PIPE,timeout);

	if(cr->admitted==0){
		return -1;
	}

	socket->type = SOCKET_PEER;
	socket->refcount--;

	free(cr);
	
	return 0;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	FCB* fcb = get_fcb(sock);
	socket_cb* socket = fcb->streamobj;

	if(socket == NULL || sock == NOFILE || socket->type != SOCKET_PEER){
		return -1;
	}

	switch (how)
	{
		case SHUTDOWN_READ:

			return pipe_close_reader(socket->peer_s.read_pipe);
			break;
		case SHUTDOWN_WRITE:

			return pipe_close_writer(socket->peer_s.write_pipe);
			break;
		case SHUTDOWN_BOTH:

			pipe_close_writer(socket->peer_s.write_pipe);
			return pipe_close_reader(socket->peer_s.read_pipe);
			break;
		default:
			assert(0);
			break;
	}
}



int socket_read(void* this, char* buffer, unsigned int size){
	socket_cb* socket = (socket_cb*)this;
	
	if(socket==NULL){
		return -1;
	}


	return pipe_read(socket->peer_s.read_pipe, buffer, size);
}

int socket_write(void* this, const char* buffer, unsigned int size){
	socket_cb* socket = (socket_cb*)this;

	if(socket==NULL){
		return -1;
	}

	return pipe_write(socket->peer_s.write_pipe,buffer,size);
}

int socket_close(void* this){

	socket_cb* socket = this;

	if(socket == NULL){
		return -1;
	}

	PORT_MAP[socket->port] = NULL;

	return 0;
}