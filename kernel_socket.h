#include "tinyos.h"
#include "kernel_streams.h"
#include "kernel_proc.h"
#include "kernel_cc.h"
#include "kernel_sched.h"


typedef struct socket_control_block socket_cb;

int socket_read(void* this, char* buffer, unsigned int size);
int socket_write(void* this, const char* buffer, unsigned int size);
int socket_close(void* this);

static file_ops socket_operations = {
    .Open = NULL,
    .Read = socket_read,
    .Write = socket_write,
    .Close = socket_close
};


typedef struct lsocket{
    rlnode queue;
    CondVar req_available;
}listener_socket;

typedef struct usocket{
    rlnode unbound_socket;
}unbound_socket;


typedef struct psocket{
    socket_cb* peer;
    pipe_cb* read_pipe;
    pipe_cb* write_pipe;
}peer_socket;

typedef enum stype{
    SOCKET_LISTENER,
    SOCKET_UNBOUND,
    SOCKET_PEER
}socket_type;


typedef struct socket_control_block{
    unsigned int refcount;
    FCB* fcb;
    socket_type type;
    port_t port;

    union{
        listener_socket listener_s;
        unbound_socket unbound_s;
        peer_socket peer_s;
    };

}socket_cb;
