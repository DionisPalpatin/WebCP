#ifndef __HANDLER_H__
#define __HANDLER_H__


#include "logger.h"


#define FILE_BLOCK_SIZE 1024

#define FORBIDDEN          403
#define NOT_FOUND          404
#define METHOD_NOT_ALLOWED 405

#define STATIC_ROOT "/home/daemo/Документы/University/WebsCP/static"


int handle_client(struct pollfd *client, char *req_filename, char *method, long *offset);
int read_request(int client_socket, char *req_filename, char *res_method);
void send_headers(long file_size, char *file_path, int client_socket);
int send_response(int client_socket, const char* file_path, const char* method, long *offset);
void send_error(int client_socket, int status_code);


#endif