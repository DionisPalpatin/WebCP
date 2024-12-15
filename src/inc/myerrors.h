#ifndef __MYERRORS_H__
#define __MYERRORS_H__


typedef enum {
    OK,

    LOG_DIR_OPEN_ERR,
    MAIN_LOG_FILE_OPEN_ERR,
    STEPS_LOG_FILE_OPEN_ERR,

    SOCKET_ERR,
    SOCK_OPTS_ERR,
    SET_NONBLOCKING_ERR,
    BIND_SOCKET_ERR,
    LISTEN_ERR,

    FORK_ERR,
    ACCEPT_ERR,

    POLL_TIMEOUT_ERR,
    REVENT_ERR,

    METHOD_ERR,
    FORBIDDEN_ERR,
    DIR_INSTEAD_FILE_ERR,

    READ_ERR,
    SEND_ERR,
    FILE_OPEN_ERR,
    SEEK_ERR
} my_errors;


const char* error_to_string(my_errors error_code);


#endif