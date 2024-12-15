#include <sys/socket.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <errno.h>

#include "../inc/handler.h"
#include "../inc/myerrors.h"
#include "../inc/http.h"


int get_file_size(int fd, long *size) {
    log_message(LOG_STEPS, "get_file_size()\n");

    *size = lseek(fd, 0, SEEK_END);
    if (*size == (off_t)-1)
        return -1;

    return 0;
}


int get_file_size_with_open(connection_t *conn, long *size) {
    log_message(LOG_STEPS, "get_file_size_with_open()\n");

    int fd = open(conn->path, O_RDONLY);
    if (fd == -1) {
        log_message(LOG_ERROR, "Error while opening file in process_file()\n");
        send_error(conn, NOT_FOUND);  
        conn->state = ERROR;

        return -1;
    }

    *size = lseek(fd, 0, SEEK_END);
    if (*size == (off_t)-1) {
        log_message(LOG_ERROR, "Error while counting file size\n");
        send_error(conn, INTERNAL_SERVER_ERROR);  
        conn->state = ERROR;
        close(fd);

        return -1;
    }

    close(fd);
    return 0;
}


int read_file(connection_t *conn) {
    log_message(LOG_STEPS, "read_file()\n");

    int fd = open(conn->path, O_RDONLY);
    if (fd == -1) {
        log_message(LOG_ERROR, "Error while opening file in process_file()\n");
        send_error(conn, NOT_FOUND);  
        conn->state = ERROR;

        return -1;
    }

    flock(fd, LOCK_EX);

    int ok = get_file_size(fd, &conn->resp_buffer.total);
    if (!ok) {
        log_message(LOG_ERROR, "Error while count file size\n");
        send_error(conn, INTERNAL_SERVER_ERROR);                
        conn->state = ERROR;

        flock(fd, LOCK_UN);
        close(fd);

        return -1;
    }
    
    conn->resp_buffer.buffer = malloc(conn->resp_buffer.total);
    if (!conn->resp_buffer.buffer) {
        log_message(LOG_ERROR, "Error while allocate memory for file buffer\n");
        send_error(conn, INTERNAL_SERVER_ERROR);                
        conn->state = ERROR;

        flock(fd, LOCK_UN);
        close(fd);

        return -1;
    }

    ssize_t bytes = read(fd, conn->resp_buffer.buffer, conn->resp_buffer.total);
    if (bytes <= conn->resp_buffer.total) {
        log_message(LOG_ERROR, "Error while reading file\n");

        send_error(conn, INTERNAL_SERVER_ERROR);
        conn->state = ERROR;

        flock(fd, LOCK_UN);
        close(fd);

        return -1;
    }

    flock(fd, LOCK_UN);
    close(fd);

    return 0;
}


void handle_client(connection_t *conn) {
    log_message(LOG_STEPS, "handle_client()\n");

    if (conn->state == READY_FOR_REQUEST) {
        read_request(conn);
    } else if (conn->state == READY_FOR_RESPONSE) {
        send_response_file(conn);
    }
}


void read_request(connection_t *conn) {
    log_message(LOG_STEPS, "read_request()\n");

    ssize_t recv_bytes = recv(conn->socket, conn->req_buffer.buffer, MAX_REQUEST_SIZE_BYTES, 0);
    if (recv_bytes <= 0) {
        log_message(LOG_ERROR, "Receive HTTP request failed\n");
        conn->state = ERROR;
        return;
    }

    parse_http_request(conn);

    log_message(LOG_DEBUG, "After parsing http request we have: state == %d, path == %s, method == %s\n", conn->state, conn->path, conn->method);

    if (conn->state != COMPLETE && conn->state != ERROR) {
        log_message(LOG_DEBUG, "Received method: %d\n", conn->method);
        if (conn->method == HEAD)
            send_response_headers(conn, -1);
        else
            send_response_file(conn);
    }

    return;
}


void send_response_headers(connection_t *conn, long file_size) {
    log_message(LOG_STEPS, "send_response_headers()\n");

    if (file_size < 0) {
        if (get_file_size_with_open(conn, &file_size))
            return;
    }
    
    char headers[1024];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n", file_size);

    const char* content_type = "text/plain";
    const char* file_ext = strrchr(conn->path, '.');

    if (file_ext != NULL) {
        if (strcmp(file_ext, ".html") == 0) {
            content_type = "text/html";
        }
        else if (strcmp(file_ext, ".css") == 0) {
            content_type = "text/css";
        }
        else if (strcmp(file_ext, ".js") == 0) {
            content_type = "application/javascript";
        }
        else if (strcmp(file_ext, ".png") == 0) {
            content_type = "image/png";
        }
        else if (strcmp(file_ext, ".jpg") == 0 || strcmp(file_ext, ".jpeg") == 0) {
            content_type = "image/jpeg";
        }
        else if (strcmp(file_ext, ".swf") == 0) {
            content_type = "application/x-shockwave-flash";
        }
        else if (strcmp(file_ext, ".gif") == 0) {
            content_type = "image/gif";
        }
    }

    sprintf(headers + strlen(headers), "Content-Type: %s\r\n\r\n", content_type);

    ssize_t sent_bytes = send(conn->socket, headers, strlen(headers), 0);
    if (sent_bytes <= 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            log_message(LOG_ERROR, "EAGAION or EWOULDBLOCK happend in send_response_headers()\n");
            return;
        }

        log_message(LOG_ERROR, "Error while send() in send_response_headers(), sent %ld bytes\n", sent_bytes);
        conn->state = ERROR;
    } else {
        log_message(LOG_INFO, "Successfully sent headers\n");
        conn->state = COMPLETE;
    }
}


void send_response_file(connection_t *conn) {
    log_message(LOG_STEPS, "send_response_file()\n");

    int ok = read_file(conn);
    if (!ok)
        return;

    if (conn->resp_buffer.offset == 0) {
        send_response_headers(conn, conn->resp_buffer.total);
        if (conn->state != COMPLETE)
            return;
    }

    conn->state = READY_FOR_RESPONSE;

    ssize_t sent_bytes = send(conn->socket, conn->resp_buffer.buffer + conn->resp_buffer.offset, RESPONSE_BLOCK_SIZE_BYTES, 0);

    if (sent_bytes < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            log_message(LOG_ERROR, "EAGAION or EWOULDBLOCK happend in send_response_headers()\n");
            return;
        }

        log_message(LOG_ERROR, "Error while send() in send_response_file(), sent %ld bytes\n", sent_bytes);
        conn->state = ERROR;

        return;
    }

    log_message(LOG_INFO, "Successfully sent file part\n");
    conn->resp_buffer.offset += sent_bytes;
    
    if (conn->resp_buffer.offset == conn->resp_buffer.total) {
        log_message(LOG_INFO, "Successfully sent full file\n");
        conn->state = COMPLETE;
    }
}


void send_error(connection_t *conn, int error) {
    log_message(LOG_STEPS, "send_error()\n");

    char response[1024];
    sprintf(response, "HTTP/1.1 %d\r\nContent-Length: 0\r\nContent-Type: text/html\r\n\r\n", error);

    log_message(LOG_INFO, "HTTP ERROR %s\n", response);

    ssize_t sent_bytes = send(conn->socket, response, strlen(response), 0);

    if (sent_bytes <= 0) {
        log_message(LOG_ERROR, "Error while send() in send_error(), sent %ld bytes\n", sent_bytes);
        conn->state = ERROR;
    } else {
        log_message(LOG_INFO, "Successfully sent error\n");
        conn->state = COMPLETE;
    }
}