#ifndef __SERVER_H__
#define __SERVER_H__


#include "handler.h"
#include "logger.h"


#define PORT            1237
#define MAX_CLIENTS     1000
#define NUM_CHILDREN    5
#define TIMEOUT         1


typedef enum {
    READ_OP,
    WRITE_OP
} operation_t;


int init_server(struct sockaddr_in *server_addr, int *server_socket);
int worker(struct sockaddr_in *server_addr, int *server_socket);
int server(void);


#endif // __SERVER_H__