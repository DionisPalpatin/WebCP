#include <linux/limits.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

#include "../inc/http.h"
#include "../inc/myerrors.h"


int get_executable_dir(char *buffer, size_t buffer_size) {
    log_message(LOG_STEPS, "get_executable_dir()\n");

    ssize_t len = readlink("/proc/self/exe", buffer, buffer_size - 1);
    if (len == -1) {
        log_message(LOG_ERROR, "readlink() error in get_executable_dir()\n");
        return -1;
    }

    buffer[len] = '\0';

    char *last_slash = strrchr(buffer, '/');
    if (last_slash != NULL) {
        *(last_slash + 1) = '\0';
    }

    return 0;
}


int check_if_file(const char *path) {
    log_message(LOG_STEPS, "check_if_file()\n");

    struct stat path_stat;
    if (stat(path, &path_stat) == -1) {
        log_message(LOG_ERROR, "stat() error in check_if_file()\n");
        return -1;
    }

    // Проверяем, что это файл (а не директория)
    if (S_ISREG(path_stat.st_mode)) {
        log_message(LOG_INFO, "It is a file: %s\n", path);
        return 1;
    } else if (S_ISDIR(path_stat.st_mode)) {
        log_message(LOG_ERROR, "It is a directory, not a file: %s\n", path);
        return 0;
    } else {
        log_message(LOG_ERROR, "Not a regular file or directory: %s\n", path);
        return -1;
    }
}


void parse_http_request(connection_t *conn) {
    log_message(LOG_STEPS, "begin parse_http_request()\n");

    char method[METHOD_NAME_LEN], url_path[PATH_MAX], protocol[PROTOCOL_NAME_LEN];

    sscanf(conn->req_buffer.buffer, "%s %s %s", method, url_path, protocol);

    // Проверяем метод
    if (strcmp(method, "GET") == 0) {
        conn->method = GET;
    } else if (strcmp(method, "HEAD") == 0) {
        conn->method = HEAD;
    } else {
        log_message(LOG_ERROR, "Unsupported HTTP method was received\n");
        send_error(conn, METHOD_NOT_ALLOWED);
        conn->state = ERROR;
        return;
    }

    char static_path[PATH_MAX];
    int error = get_executable_dir(static_path, PATH_MAX);
    if (error) {
        conn->state = ERROR;
        return;
    }

    strcat(static_path, "static");

    if (strlen(url_path) + strlen(static_path) > PATH_MAX) {
        send_error(conn, NOT_FOUND);
        conn->state = ERROR;
        return;
    }
    
    strcat(static_path, url_path);
    if (strcmp(url_path, "/") == 0) {
        strcat(static_path, "index.html");
    }

    log_message(LOG_DEBUG, "URL path: %s\n", url_path);
    log_message(LOG_DEBUG, "Static path: %s\n", static_path);

    int is_file = check_if_file(static_path);

    log_message(LOG_DEBUG, "is_file: %d\n", is_file);

    if (is_file != 1) {
        log_message(LOG_ERROR, "Requested resourse is not file\n");
        send_error(conn, FORBIDDEN);
        conn->state = ERROR;

        return;
    }

    log_message(LOG_DEBUG, "If conn is not null: %d\n", conn != NULL);

    strncpy(conn->path, static_path, PATH_MAX);
    
    log_message(LOG_DEBUG, "Path buffer in conn: %s\n", conn->path);
    
    log_message(LOG_STEPS, "end parse_http_request()\n");
}