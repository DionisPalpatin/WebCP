int handle_client(struct pollfd *client, char *req_filename, char *method, long *offset) {   
    int client_socket = client->fd;
    short revents = client->revents;
    short events = client->events;
    if (revents & events == 0) {
        log_message(LOG_ERROR, "Unsupported operation(-s) on client with socket %d\n", client_socket);
        return REVENT_ERR;
    }
    if (revents & POLLIN) {
        return read_request(client_socket, req_filename, method);
    } 
    if (revents & POLLOUT) {
        return send_response(client_socket, req_filename, method, offset);
    }
}
int read_request(int client_socket, char *req_filename, char *res_method) {
    char buffer[1024];
    char method[10], path[PATH_MAX], protocol[10];
    if (recv(client_socket, buffer, sizeof(buffer), 0) <= 0) {
        perror("Receive failed\n");
        return OK;
    }
    sscanf(buffer, "%s %s %s", method, path, protocol);
    if (strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        send_error(client_socket, METHOD_NOT_ALLOWED);
        return OK;
    }
    char full_path[PATH_MAX];
    sprintf(full_path, "%s%s", STATIC_ROOT, path);
    char resolved_path[PATH_MAX];
    if (realpath(full_path, resolved_path) == NULL) {
        send_error(client_socket, NOT_FOUND);
        return OK;
    }
    if (strncmp(resolved_path, STATIC_ROOT, strlen(STATIC_ROOT)) != 0) {
        send_error(client_socket, FORBIDDEN);
        return OK;
    }
    if (strcmp(path, "/") == 0) {
        strcat(full_path, "index.html");
    }
    struct stat path_stat;
    if (stat(full_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
        send_error(client_socket, NOT_FOUND);
        return OK;
    } else {
        strncpy(req_filename, full_path, PATH_MAX);
        strncpy(res_method, method, 10);
    }
    return OK;
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
    } else if (offset < file_size) {
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
int worker(struct sockaddr_in *server_addr, int server_socket) {
	int error = OK;
	struct pollfd fds[MAX_CLIENTS + 1];
	int nfds = 1;
	char paths[MAX_CLIENTS][PATH_MAX];
	char methods[MAX_CLIENTS][10];
	long offsets[MAX_CLIENTS] = {0};
	fds[0].fd = server_socket;
	fds[0].events = POLLIN;
	for (int i = 1; i <= MAX_CLIENTS; i++)
		fds[i].fd = -1;
	log_message(LOG_INFO, "Poll initialized successfully\n");
	while (!error) {
		int new_conn = poll(fds, nfds, TIMEOUT);
		if (new_conn < 0) {
			log_message(LOG_ERROR, "poll() error in worker()\n");
			error = FORK_ERR;
			continue;
		} else if (new_conn == 0) {
			log_message(LOG_INFO, "poll() timed out. End program.\n");
			error = POLL_TIMEOUT_ERR;
			continue;
		}
	}
	for (int i = 0; i < nfds && !error; i++) {
		if(fds[i].revents == 0)
			continue;
		if (fds[i].fd == server_socket) {
			if ((fds[i].revents & POLLIN) == 0) {
				error = REVENT_ERR;
				log_message(LOG_ERROR, "Unsupportable revents on server socket: %d\n", fds[i].revents);
			} else {
				error = accept_new_clients(server_socket, fds, &nfds);
			}
		} else  {
			error = handle_client(&fds[i], paths[i - 1], methods[i - 1], offsets[i - 1]);
		}
	}
	return error;
}
int server(void) {
	int server_socket;
	struct sockaddr_in server_addr;
	int error = init_server(&server_addr, &server_socket);
	if (!error) {
		for (size_t i = 0; i < NUM_CHILDREN && !error; i++) {
			log_message(LOG_INFO, "Try to fork()...\n");
			pid_t pid = fork();
			if (pid < 0) {
				error = FORK_ERR;
				log_message(LOG_ERROR, "fork() error in worker()\n");
				break;
			}
			log_message(LOG_INFO, "fork() is executed successfully.\n");
			if (pid == 0)
				error = worker(&server_addr, server_socket);
				exit(error);
		}
	}
	return error;
}