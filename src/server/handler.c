#include <sys/socket.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <poll.h>

#include "../inc/handler.h"
#include "../inc/myerrors.h"


int handle_client(struct pollfd *client, char *req_filename, char *method, long *offset) {   
    int client_socket = client->fd;
    short revents = client->revents;
    short events = client->events;

    if ((revents & events) == 0) {
        log_message(LOG_ERROR, "Unsupported operation(-s) on client with socket %d\n", client_socket);
        return REVENT_ERR;
    }
    
    if (revents & POLLIN) {
        return read_request(client_socket, req_filename, method);
    }
    
    if (revents & POLLOUT) {
        int result = send_response(client_socket, req_filename, method, offset);

        // Если все данные (заголовок или и данные тоже, не важно) переданы, можно закрыть соединение
        if (result == OK && *offset == 0) {
            close(client_socket);
        }
    }
}


int read_request(int client_socket, char *req_filename, char *res_method) {
    char buffer[1024];
    char method[10], path[PATH_MAX], protocol[10];

    // Получение запроса
    if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
        perror("Receive failed\n");
        return OK;
    }

    // Парсинг запроса
    sscanf(buffer, "%s %s %s", method, path, protocol);
    
    // Поддерживаются только GET и HEAD запросы
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        send_error(client_socket, METHOD_NOT_ALLOWED);
        return OK;
    }

    // Строим полный путь
    char full_path[PATH_MAX];
    sprintf(full_path, "%s%s", STATIC_ROOT, path);

    char resolved_path[PATH_MAX];
    if (realpath(full_path, resolved_path) == NULL) {
        send_error(client_socket, NOT_FOUND);
        return OK;
    }

    // Защита от несанкционированного доступа
    if (strncmp(resolved_path, STATIC_ROOT, strlen(STATIC_ROOT)) != 0) {
        send_error(client_socket, FORBIDDEN);
        return OK;
    }

    if (strcmp(path, "/") == 0) {
        strcat(full_path, "index.html");
    }

    struct stat path_stat;
    // Если это директория, отправляем список файлов и поддиректорий
    if (stat(full_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        send_error(client_socket, NOT_FOUND);
        return OK;
    } else {
        strncpy(req_filename, full_path, PATH_MAX);
        strncpy(res_method, method, 10);
    }

    return OK;
}


void send_headers(long file_size, char *file_path, int client_socket) {
    char headers[1024];
    sprintf(headers, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n", file_size);

    const char* content_type = "text/plain";
    const char* file_ext = strrchr(file_path, '.');
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

    send(client_socket, headers, strlen(headers), 0);
}


int send_response(int client_socket, const char* file_path, const char* method, long *offset) {
    FILE* file = fopen(file_path, "rb");
    if (file == NULL) {
        send_error(client_socket, NOT_FOUND);
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);

    if (strcmp(method, "HEAD") == 0) {
        send_headers(file_size, file_path, client_socket);
    } else if (offset < file_size) {
        // Если файл еще не передавался, передаем еще и заголовки сначала
        if (*offset == 0) {
            send_headers(file_size, file_path, client_socket);
        }

        // Отправка содержимого файла
        long block_size = (file_size - *offset >= FILE_BLOCK_SIZE) ? FILE_BLOCK_SIZE : file_size - *offset;

        fseek(file, *offset, SEEK_SET);
        char buffer[block_size];

        size_t bytes_read = fread(buffer, block_size, sizeof(buffer), file);
        if (bytes_read > 0)
            send(client_socket, buffer, bytes_read, 0);

        *offset += block_size;

        if (*offset == file_size)
            *offset = 0;
    }

    fclose(file);
}


void send_error(int client_socket, int status_code) {
    char response[1024];
    sprintf(response, "HTTP/1.1 %d\r\nContent-Length: 0\r\nContent-Type: text/html\r\n\r\n", status_code);

    log_message(LOG_INFO, "ERROR %s\n", response);
    log_message(LOG_DEBUG, "Debug message: %d\n", 42);

    send(client_socket, response, strlen(response), 0);
}