#ifndef __MYERRORS_H__
#define __MYERRORS_H__


typedef enum {
    OK,

    SOCKET_ERR,
    SOCK_OPTS_ERR,
    SET_NONBLOCKING_ERR,
    BIND_SOCKET_ERR,
    LISTEN_ERR,

    SEND_ERR,
    RECIEVE_ERR,
    FORK_ERR,
    ACCEPT_ERR,

    POLL_ERR,
    POLL_TIMEOUT_ERR,
    REVENT_ERR,

    METHOD_ERR,
    FORBIDDEN_ERR,
    DIR_INSTEAD_FILE_ERR,

    READ_ERR
} my_errors;


#endif