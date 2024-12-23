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
    log_message(LOG_STEPS, "BEGIN get_file_size()\n");

    *size = lseek(fd, 0, SEEK_END);
    if (*size == (off_t)-1) {
        log_message(LOG_ERROR, "Error while counting file size in get_file_size(): %s\n", strerror(errno));
        return -1;
    }

    lseek(fd, 0, SEEK_SET);

    log_message(LOG_STEPS, "END get_file_size()\n");    

    return 0;
}


int get_file_size_with_open(connection_t *conn, long *size) {
    log_message(LOG_STEPS, "BEGIN get_file_size_with_open()\n");

    int fd = open(conn->path, O_RDONLY);
    if (fd == -1) {
        log_message(LOG_ERROR, "Error while opening file in get_file_size_with_open()\n");
        send_error(conn, NOT_FOUND);  
        conn->state = ERROR;

        return -1;
    }

    *size = lseek(fd, 0, SEEK_END);
    if (*size == (off_t)-1) {
        log_message(LOG_ERROR, "Error while counting file size in get_file_size_with_open(): %s\n", strerror(errno));
        send_error(conn, INTERNAL_SERVER_ERROR);  
        conn->state = ERROR;
        
        close(fd);

        return -1;
    }

    lseek(fd, 0, SEEK_SET);
    close(fd);

    log_message(LOG_STEPS, "END get_file_size_with_open()\n");

    return 0;
}


int read_file(connection_t *conn) {
    log_message(LOG_STEPS, "BEGIN read_file()\n");

    log_message(LOG_DEBUG, "read_file(), conn->path: %s\n", conn->path);

    int fd = open(conn->path, O_RDONLY);
    if (fd == -1) {
        log_message(LOG_ERROR, "Error while opening file in read_file(): %s\n", strerror(errno));
        send_error(conn, NOT_FOUND);  
        conn->state = ERROR;

        return -1;
    }

    if (get_file_size(fd, &conn->resp_buffer.file_size)) {
        log_message(LOG_ERROR, "Error while get file size: %s\n", strerror(errno));
        send_error(conn, INTERNAL_SERVER_ERROR);
        conn->state = ERROR;

        close(fd);

        return -1;
    }

    lseek(fd, conn->resp_buffer.offset, SEEK_SET);

    ssize_t bytes = read(fd, conn->resp_buffer.buffer, RESPONSE_BLOCK_SIZE_BYTES);
    if (bytes < 0) {
        log_message(LOG_ERROR, "Error while reading file: %s\n", strerror(errno));
        send_error(conn, INTERNAL_SERVER_ERROR);
        conn->state = ERROR;

        close(fd);

        return -1;
    } else {
        conn->resp_buffer.total = bytes;
    }

    close(fd);

    log_message(LOG_STEPS, "END read_file()\n");

    return 0;
}


void handle_client(connection_t *conn) {
    log_message(LOG_STEPS, "BEGIN handle_client()\n");

    if (conn->state == READY_FOR_REQUEST) {
        read_request(conn);
    } else if (conn->state == READY_FOR_RESPONSE) {
        send_response_file(conn);
    }

    log_message(LOG_STEPS, "END handle_client()\n");
}


void read_request(connection_t *conn) {
    log_message(LOG_STEPS, "BEGIN read_request()\n");

    log_message(LOG_DEBUG, "conn->socket: %d\n", conn->socket);

    if (conn->socket < 0) {
        log_message(LOG_ERROR, "Invalid socket in read_request()\n");
        conn->state = ERROR;
        return;
    }

    ssize_t recv_bytes = recv(conn->socket, conn->req_buffer.buffer, MAX_REQUEST_SIZE_BYTES, 0);
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
        log_message(LOG_DEBUG, "No data available yet for reading\n");
        return;
    } else if (recv_bytes <= 0) {
        log_message(LOG_ERROR, "Receive HTTP request failed: %s\n", strerror(errno));
        conn->state = ERROR;
        return;
    }

    parse_http_request(conn);

    log_message(LOG_DEBUG, "After parsing http request we have: state == %d, path == %s, method == %d\n", conn->state, conn->path, conn->method);

    if (conn->state != COMPLETE && conn->state != ERROR) {
        log_message(LOG_DEBUG, "Received method: %d\n", conn->method);
        if (conn->method == HEAD)
            send_response_headers(conn, -1);
        else
            send_response_file(conn);
    }

    log_message(LOG_STEPS, "END read_request()\n");
}


void send_response_headers(connection_t *conn, long file_size) {
    log_message(LOG_STEPS, "BEGIN send_response_headers()\n");

    if (file_size < 0) {
        if (get_file_size_with_open(conn, &file_size))
            return;
    }
    
    char headers[1024];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n", file_size);

    const char* content_type = "text/plain";
    const char* file_ext = strrchr(conn->path, '.');

    log_message(LOG_DEBUG, "File ext in send_response_headers(): %s\n", file_ext);

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

    log_message(LOG_DEBUG, "Headers string in send_response_headers(): %s\n", headers);

    ssize_t sent_bytes = send(conn->socket, headers, strlen(headers), 0);
    if (sent_bytes <= 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            log_message(LOG_ERROR, "EAGAION or EWOULDBLOCK happend in send_response_headers()\n");
            return;
        }

        log_message(LOG_ERROR, "Error while send() in send_response_headers(), sent %ld bytes: %s\n", sent_bytes, strerror(errno));
        conn->state = ERROR;
    } else {
        log_message(LOG_INFO, "Successfully sent headers\n");
        conn->state = COMPLETE;
    }

    log_message(LOG_STEPS, "END send_response_headers()\n");
}


void send_response_file(connection_t *conn) {
    log_message(LOG_STEPS, "BEGIN send_response_file()\n");

    if (conn->resp_buffer.buffer == NULL)
        return;

    if (read_file(conn))
        return;

    log_message(LOG_DEBUG, "send_response_file(), offset: %d, total: %d\n", conn->resp_buffer.offset, conn->resp_buffer.total);    

    if (conn->resp_buffer.offset == 0) {
        send_response_headers(conn, conn->resp_buffer.file_size);
        if (conn->state != COMPLETE)
            return;
    }

    conn->state = READY_FOR_RESPONSE;

    size_t send_buf_len = RESPONSE_BLOCK_SIZE_BYTES;
    if (conn->resp_buffer.total - conn->resp_buffer.offset < RESPONSE_BLOCK_SIZE_BYTES)
        send_buf_len = conn->resp_buffer.file_size - conn->resp_buffer.offset;

    ssize_t sent_bytes = send(conn->socket, conn->resp_buffer.buffer + conn->resp_buffer.offset, send_buf_len, 0);

    if (sent_bytes < 0) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            log_message(LOG_ERROR, "EAGAION or EWOULDBLOCK happend in send_response_headers()\n");
            return;
        }

        log_message(LOG_ERROR, "Error while send() in send_response_file(), sent %ld bytes: %s\n", sent_bytes, strerror(errno));
        conn->state = ERROR;

        return;
    }

    log_message(LOG_INFO, "Successfully sent file part\n");
    conn->resp_buffer.offset += sent_bytes;
    
    if (conn->resp_buffer.offset == conn->resp_buffer.file_size) {
        log_message(LOG_INFO, "Successfully sent full file\n");
        conn->state = COMPLETE;
    }

    log_message(LOG_STEPS, "END send_response_file()\n");
}


void send_error(connection_t *conn, int error) {
    log_message(LOG_STEPS, "BEGIN send_error()\n");

    char response[1024];
    sprintf(response, "HTTP/1.1 %d\r\nContent-Length: 0\r\nContent-Type: text/html\r\n\r\n", error);

    log_message(LOG_INFO, "HTTP ERROR %s\n", response);

    ssize_t sent_bytes = send(conn->socket, response, strlen(response), 0);

    if (sent_bytes <= 0) {
        log_message(LOG_ERROR, "Error while send() in send_error(), sent %ld bytes: %s\n", sent_bytes, strerror(errno));
        conn->state = ERROR;
    } else {
        log_message(LOG_INFO, "Successfully sent error\n");
        conn->state = COMPLETE;
    }

    log_message(LOG_STEPS, "END send_error()\n");
}
