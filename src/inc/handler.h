#ifndef __HANDLER_H__
#define __HANDLER_H__


#include <linux/limits.h>

#include "logger.h"


#define FILE_BLOCK_SIZE 1024
#define MAX_REQUEST_SIZE_BYTES 2048
#define RESPONSE_BLOCK_SIZE_BYTES 4096


typedef enum {
    READY_FOR_REQUEST,
    READY_FOR_RESPONSE,
    COMPLETE,
    ERROR
} state_t;


typedef enum {
    HEAD,
    GET
} methods_t;


typedef struct {
    char *buffer;
    long offset;
    long total;
    long file_size;
} response_buffer_t;


typedef struct {
    // char buffer[MAX_REQUEST_SIZE_BYTES];
    char *buffer;
} request_buffer_t;


typedef struct {
    response_buffer_t resp_buffer;
    request_buffer_t req_buffer;

    // char path[PATH_MAX];
    char *path;
    methods_t method;

    int state;
    int socket;
} connection_t;


int get_file_size(int fd, long *size);
int get_file_size_with_open(connection_t *conn, long *size);
int read_file(connection_t *conn);
void handle_client(connection_t *conn);
void read_request(connection_t *conn);
void send_response_headers(connection_t *conn, long file_size);
void send_response_file(connection_t *conn);
void send_error(connection_t *conn, int error);


#endif