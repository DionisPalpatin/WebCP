#include "../inc/myerrors.h"


const char* error_to_string(my_errors error_code) {
    switch (error_code) {
        case OK:
            return "OK";

        case LOG_DIR_OPEN_ERR:
            return "Log directory open error";

        case MAIN_LOG_FILE_OPEN_ERR:
            return "Main log file open error";

        case STEPS_LOG_FILE_OPEN_ERR:
            return "Steps log file open error";

        case SOCKET_ERR:
            return "Socket error";

        case SOCK_OPTS_ERR:
            return "Socket options error";

        case SET_NONBLOCKING_ERR:
            return "Set non-blocking mode error";

        case BIND_SOCKET_ERR:
            return "Bind socket error";

        case LISTEN_ERR:
            return "Listen error";

        case FORK_ERR:
            return "Fork error";

        case ACCEPT_ERR:
            return "Accept error";

        case POLL_TIMEOUT_ERR:
            return "Poll timeout error";

        case REVENT_ERR:
            return "Revent error";

        case METHOD_ERR:
            return "Method error";

        case FORBIDDEN_ERR:
            return "Forbidden error";

        case DIR_INSTEAD_FILE_ERR:
            return "Directory instead of file error";

        case READ_ERR:
            return "Read error";

        case SEND_ERR:
            return "Send error";

        case FILE_OPEN_ERR:
            return "File open error";

        case SEEK_ERR:
            return "Seek error";

        default:
            return "Unknown error";
    }
}