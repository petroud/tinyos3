#include "tinyos.h"
#include "kernel_socket.h"
#include "kernel_streams.h"

//The table of ports on which sockets can be created
socket_cb* PORT_MAP[MAX_PORT+1];

//Allocates new a socket and bonds it with FCBs reserved in the OS 
Fid_t sys_Socket(port_t port)
{
	if(port < NOPORT || port> MAX_PORT){
		return -1;
	}

	socket_cb* socket = xmalloc(sizeof(socket_cb));

	FCB** fcbs = (FCB**)xmalloc(sizeof(FCB*));
	Fid_t* fids = (Fid_t*)xmalloc(sizeof(Fid_t));

	//Reserve an FCB in FIDT and bond it with an Fid_t
	//which is the identity of the socket
	int retval = FCB_reserve(1,fids,fcbs);

	if(retval == 0){
		return -1;
	}

	socket->port = port;

	//The socket is initially unbounded
	socket->type = SOCKET_UNBOUND;
	socket->fcb = fcbs[0];
	socket->refcount = 0;

	fcbs[0]->streamobj = socket;
	fcbs[0]->streamfunc = &socket_operations;

	//Return the new fid
	return fids[0];
}


// Promotes the socket corresponding to Fid_t #sock to 
// be a listener. Some checks for errors are being made 
// and then the type of socket is changed to LISTENER. The 
// queue of request is initialized and the Cond_Var that is a flag
// for new requests arriving is being set to COND_INIT. The socket 
// is put at last in PORT_MAP table.
int sys_Listen(Fid_t sock)
{

	FCB* fcb = get_fcb(sock);

	if(fcb==NULL){
		return -1;
	}

	socket_cb* socket = (socket_cb*)fcb->streamobj;

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

//Accept waits until a request arrives in the queue of requests of the listening socket
//When a requests arrives,the socket bound to it is promoted to PEER and gets connected 
//with the corresponding socket accessed from the PORT_MAP which is also now a PEER. Two new pipes
//are respectively the writer/reader (and vice versa) of each other. After the two sockets
//gets connected with "ring" the Cond_Var that we have a connection.
Fid_t sys_Accept(Fid_t lsock)
{

	FCB* fcb = get_fcb(lsock);

	if(fcb == NULL){
		return -1;
	}

	socket_cb* socket_listens = fcb->streamobj;


	if(socket_listens == NULL ){
		return -1;
	}

	if(socket_listens->type != SOCKET_LISTENER){
		return -1;
	}

	socket_listens->refcount++;


	while(is_rlist_empty(&socket_listens->listener_s.queue) && PORT_MAP[socket_listens->port]!=NULL){
		kernel_wait(&socket_listens->listener_s.req_available, SCHED_PIPE);
	}

	if(PORT_MAP[socket_listens->port]==NULL){
		return -1;
	}

	///////////////////////////////////
	rlnode* connectionNode = rlist_pop_front(&socket_listens->listener_s.queue);
	connection_request* cr = (connection_request*)connectionNode->obj;


	Fid_t fid_target = sys_Socket(socket_listens->port);
	FCB* fcb_target = get_fcb(fid_target);

	if(fcb_target==NULL){
		return -1;
	}

	cr->admitted=1;
	
	socket_cb* socket_target = fcb_target->streamobj;

	socket_target->type = SOCKET_PEER;
	cr->peer->type = SOCKET_PEER;

	socket_target->peer_s.peer = cr->peer;
	cr->peer->peer_s.peer = socket_target;

	pipe_cb* pipe1 = pipe_init();
	pipe_cb* pipe2 = pipe_init();

	pipe1->reader = socket_target->fcb;
	pipe1->writer = cr->peer->fcb;

	pipe2->reader = cr->peer->fcb;
	pipe2->writer = socket_target->fcb;

	cr->peer->peer_s.write_pipe = pipe1;
	cr->peer->peer_s.read_pipe = pipe2;	

	socket_target->peer_s.write_pipe = pipe2;
	socket_target->peer_s.read_pipe = pipe1;

	kernel_broadcast(&cr->connected_cv);
	socket_listens->refcount--;

	return fid_target;
}


//It creates a connection request and prepares a socket for connection.
//After the creation of it and the bonding with the socket corresponding 
//to Fid_t #sock, the request is being pushed back in the list of the 
//requests of the listening socket. We ring the Cond_Var that a new 
//request waits. We wait till connection is established. An error
//may have occured and the request was not admitted, in that case we 
//can do nothing and we return error. Else we have a connection and the socket
//#sock is promoted to PEER.
int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	FCB* fcb = get_fcb(sock);

	socket_cb* socket = fcb->streamobj;
	socket_cb* socket_listens = PORT_MAP[port];


	if(port<=NOPORT || port>MAX_PORT){
		return -1;
	}

	if(sock==NOFILE || socket == NULL || socket->type != SOCKET_UNBOUND){
		return -1;
	}

	if(PORT_MAP[socket->port]!=NULL){
		return -1;
	}

	if(socket_listens==NULL){
		return -1;
	}

	if(socket_listens->type!= SOCKET_LISTENER){
		return -1;
	}


	socket->refcount++;

	connection_request* cr = xmalloc(sizeof(connection_request));
	cr->admitted = 0;
	rlnode_init(&cr->queue_node,cr);
	cr->peer = socket;
	cr->connected_cv = COND_INIT;

	rlist_push_back(&socket_listens->listener_s.queue, &cr->queue_node);

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


//Terminates the connection between two sockets
//by closing the pipes that connect them both. We have
//3 ways to do this. 
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
			//If the socket's read pipe is already closed
			//and freed we dont want to try again because
			//a Segmentation Fault is on the way!!!
			if(socket->peer_s.read_pipe!=NULL){
				pipe_close_reader(socket->peer_s.read_pipe);
				socket->peer_s.read_pipe = NULL;
			}else{
				return -1;
			}
			break;
		case SHUTDOWN_WRITE:
			//Same reason....
			if(socket->peer_s.write_pipe!=NULL){
				pipe_close_writer(socket->peer_s.write_pipe);
				socket->peer_s.write_pipe = NULL;
			}else{
				return -1;
			}
			break;
		case SHUTDOWN_BOTH:
			//Again the same....
			if(socket->peer_s.read_pipe!=NULL){
				pipe_close_reader(socket->peer_s.read_pipe);
				socket->peer_s.read_pipe = NULL;
			}else if(socket->peer_s.write_pipe!=NULL){
				pipe_close_writer(socket->peer_s.write_pipe);
				socket->peer_s.write_pipe = NULL;
			}else{
				return -1;
			}
			break;
		default:
			assert(0);
			break;
	}

	return 0;
}


//How the socket knows how to read and what to return?
int socket_read(void* this, char* buffer, unsigned int size){
	socket_cb* socket = (socket_cb*)this;
	
	if(socket==NULL){
		return -1;
	}

	return pipe_read(socket->peer_s.read_pipe, buffer, size);
}

//How the socket knows how to write and what to return?
int socket_write(void* this, const char* buffer, unsigned int size){
	socket_cb* socket = (socket_cb*)this;

	if(socket==NULL){
		return -1;
	}

	return pipe_write(socket->peer_s.write_pipe,buffer,size);
}


//Closes a socket and removes it from the PORT_MAP table
//It frees the allocated space if there are no references to 
//the socket. A socket before it closes it may be a listener 
//so if a request wait we will have the chance again to serve it. 
//So we ring the Cond_Var.
int socket_close(void* this){

	socket_cb* socket = this;

	if(socket == NULL){
		return -1;
	}
	socket->refcount--;
	if(socket->type == SOCKET_LISTENER){
		PORT_MAP[socket->port] = NULL;
		kernel_broadcast(&socket->listener_s.req_available);
	}

	if(socket->type == SOCKET_PEER){
		pipe_close_reader(socket->peer_s.read_pipe);
		pipe_close_writer(socket->peer_s.write_pipe);
	}

	if(socket->refcount==0){
		free(socket);
	}

	return 0;
}