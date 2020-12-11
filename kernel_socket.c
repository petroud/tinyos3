
#include "tinyos.h"
#include "kernel_socket.h"


socket_cb* PORT_MAP[MAX_PORT+1];

unsigned int socketsCreated = 0;


socket_cb* socket_init(port_t port){
	
	socket_cb* socket = (socket_cb*)xmalloc(sizeof(socket_cb));
	socket->fcb = NULL;
	socket->type = SOCKET_UNBOUND;

	socket->port = port;
	socket->refcount = 1;

	return socket;	
}

void init_portmap(){
	for(int i=0; i<=MAX_PORT-1; i++){
		PORT_MAP[i]=NULL;
	}
}


Fid_t sys_Socket(port_t port)
{
	if(port > MAX_PORT || port <= 0){
		return NOFILE;
	}

	if(socketsCreated==0){
		init_portmap();
	}

	FCB* fcb = NULL;
	Fid_t fid = -1;

	int retval = FCB_reserve(1,&fid,&fcb)==0;
	if(retval == 0){
		return NOFILE;
	}

	socket_cb* socket = socket_init(port);
	socket->fcb = fcb;

	fcb->streamobj = socket;
	fcb->streamfunc = &socket_operations;

	//PORT_MAP[port]=socket;

	socketsCreated++;

	fprintf(stderr,"I return %d\n", fid);
	return fid;
}



int sys_Listen(Fid_t sock)
{
	return -1;
}


Fid_t sys_Accept(Fid_t lsock)
{
	return NOFILE;
}


int sys_Connect(Fid_t sock, port_t port, timeout_t timeout)
{
	return -1;
}


int sys_ShutDown(Fid_t sock, shutdown_mode how)
{
	return -1;
}



int socket_read(void* this, char* buffer, unsigned int size){
	return -1;
}

int socket_write(void* this, const char* buffer, unsigned int size){
	return -1;
}

int socket_close(void* this){

	socket_cb* socket = this;

	if(socket == NULL){
		return -1;
	}

	PORT_MAP[socket->port] = NULL;

	return 0;
}
