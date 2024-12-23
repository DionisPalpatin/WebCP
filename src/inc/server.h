#ifndef __SERVER_H__
#define __SERVER_H__


#define _POSIX_C_SOURCE 200112L


#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>

#include "handler.h"
#include "logger.h"


#define PORT            8800
#define MAX_CLIENTS     100000
#define NUM_CHILDREN    5
#define TIMEOUT         -1


typedef enum {
    READ_OP,
    WRITE_OP
} operation_t;


int init_server(struct sockaddr_in *server_addr, int *server_socket);
int worker(int server_socket);

void add_new_client(struct pollfd *fds, connection_t *client_conns, int i, int new_conn);
void close_client(struct pollfd *fds, connection_t *client_conns, int i);
int accept_new_client(int server_socket, struct pollfd *fds, connection_t *conns, int *nfds);

void compress_arrays(struct pollfd *fds, connection_t *conns, int *nfds);

void signal_handler(int signum, siginfo_t *info, void *context);
int handle_sigint(struct sigaction *sa);

int server(void);


#endif // __SERVER_H__
