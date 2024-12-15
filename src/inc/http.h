#ifndef __HTTP_H__
#define __HTTP_H__


#include "handler.h"

#define METHOD_NAME_LEN 10
#define PROTOCOL_NAME_LEN 10

#define BAD_REQUEST           400
#define FORBIDDEN             403
#define NOT_FOUND             404
#define METHOD_NOT_ALLOWED    405
#define INTERNAL_SERVER_ERROR 500


void parse_http_request(connection_t *conn);


#endif