#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <linux/limits.h>
#include <string.h>

#include "../inc/server.h"
#include "../inc/myerrors.h"
#include "../inc/handler.h"


int init_server(struct sockaddr_in *server_addr, int *server_socket) {
	log_message(LOG_STEPS, "init_server()\n");

	int error = OK;
	
	// Создание сокета
	if ((*server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_message(LOG_ERROR, "Error init server socket: errno = %s\n", strerror(errno));
		error = SOCKET_ERR;
	}

	int enable = 1;
	if (!error && setsockopt(*server_socket, SOL_SOCKET,  SO_REUSEADDR, (char *)&enable, sizeof(enable)) < 0)
	{
		log_message(LOG_ERROR, "Error while setsockopt() in init_server(): errno = %s\n", strerror(errno));
		error = SOCK_OPTS_ERR;
	}

	int flags = fcntl(*server_socket, F_GETFL, 0);
	if (!error && fcntl(*server_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
		log_message(LOG_ERROR, "Error while set nonblocking status for server socket in init_server(): errno = %s\n", strerror(errno));
		error = SET_NONBLOCKING_ERR;
	}

	server_addr->sin_family = AF_INET;
	server_addr->sin_addr.s_addr = INADDR_ANY;
	server_addr->sin_port = htons(PORT);

	if (!error && bind(*server_socket, (struct sockaddr *)server_addr, sizeof(*server_addr)) == -1) {
		log_message(LOG_ERROR, "Error while binding server socket: errno = %s\n", strerror(errno));
		log_message(LOG_DEBUG, "socket: %d\n", *socket);
		error = BIND_SOCKET_ERR;
	}

	if (!error && listen(*server_socket, SOMAXCONN) == -1) {
		log_message(LOG_ERROR, "Error init server listening: errno = %s\n", strerror(errno));
		error = LISTEN_ERR;
	}

	if (!error) {
		log_message(LOG_INFO, "Server was started successfully\n");
		log_message(LOG_INFO, "Server is listening on port %d\n", PORT);
	}

	return error;
}


void add_new_client(struct pollfd *fds, connection_t *client_conns, int i, int new_conn) {
	log_message(LOG_STEPS, "begin add_new_client()\n");

	fds[i].fd = new_conn;
	fds[i].events = POLLIN | POLLOUT;

	client_conns[i - 1].state = READY_FOR_REQUEST;
	client_conns[i - 1].socket = new_conn;
	client_conns[i - 1].resp_buffer.offset = 0;
	client_conns[i - 1].resp_buffer.total = 0;
	client_conns[i - 1].resp_buffer.buffer = NULL;

	log_message(LOG_STEPS, "end add_new_client()\n");
}


void close_client(struct pollfd *fds, connection_t *client_conns, int i) {
	log_message(LOG_STEPS, "close_client()\n");
	log_message(LOG_INFO, "Close client's connections for client with fd = %d\n", fds[i].fd);
	
	close(fds[i].fd);
	fds[i].fd = client_conns[i - 1].socket = -1;
	free(client_conns[i - 1].resp_buffer.buffer);
}


int accept_new_clients(int server_socket, struct pollfd *fds, connection_t *client_conns, int *nfds) {
	log_message(LOG_STEPS, "accept_new_clients()\n");
	
	int cur_clients = *nfds;
	int error = OK;

	while (1) {
        int new_conn = accept(server_socket, NULL, NULL);

        if (new_conn < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Нет больше новых соединений в очереди
                break;
            }

            log_message(LOG_ERROR, "Error accepting new client: errno = %s\n", strerror(errno));
            error = ACCEPT_ERR;
            break;
        }

        if (cur_clients >= MAX_CLIENTS) {
            log_message(LOG_WARNING, "Maximum number of clients reached, refusing new connection\n");
            close(new_conn);
            break;
        }

        add_new_client(fds, client_conns, cur_clients, new_conn);
        cur_clients++;

        log_message(LOG_INFO, "New client (number %d) is connected\n", cur_clients);
    }


	log_message(LOG_INFO, "Total new clients were connected: %d\n", cur_clients - *nfds);
	*nfds = cur_clients;

	return error;
}


int worker(int server_socket) {
	log_message(LOG_STEPS, "worker()\n");

	int error = OK;

	struct pollfd fds[MAX_CLIENTS + 1];
	connection_t client_connections[MAX_CLIENTS];
	int nfds = 1;

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

		if (fds[0].revents != 0) {
			if ((fds[0].revents & POLLIN) == 0) {
				error = REVENT_ERR;
				log_message(LOG_ERROR, "Unsupportable revents on server socket: %d\n", fds[0].revents);
			} else {
				error = accept_new_clients(server_socket, fds, client_connections, &nfds);
			}
		}

		for (int i = 1; i < nfds; i++) {
			// Если ничего не произошло на сокете
			if (fds[0].revents == 0)
				continue;

			if (client_connections[i - 1].state != COMPLETE && client_connections[i - 1].state != ERROR) {
				handle_client(client_connections);

				if (client_connections[i - 1].state == COMPLETE || client_connections[i - 1].state == ERROR)
					close_client(fds, client_connections, i);
			} else if (fds[i].revents & (POLLHUP | POLLERR | POLLNVAL)) {
				close_client(fds, client_connections, i);
			}
		}
	}

	return error;
}


int server(void) {
    int server_socket;
    struct sockaddr_in server_addr;

	int error = open_logs_files();
	if (error)
		return error;

    error = init_server(&server_addr, &server_socket);
    if (error)
		return error;

    pid_t children[NUM_CHILDREN];

    for (size_t i = 0; i < NUM_CHILDREN && !error; i++) {
        log_message(LOG_INFO, "Try to fork()...\n");

        pid_t pid = fork();
        if (pid < 0) {
            error = FORK_ERR;
            log_message(LOG_ERROR, "fork() error in worker()\n");
            break;
        }

        if (pid == 0) {
            error = worker(server_socket);
            exit(error);
        } else {
            children[i] = pid;
            log_message(LOG_INFO, "Child process created with PID: %d\n", pid);
        }
    }

    for (size_t i = 0; i < NUM_CHILDREN; i++) {
        int status;
        pid_t pid = waitpid(children[i], &status, 0);
        if (pid > 0) {
            log_message(LOG_INFO, "Child process with PID %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else {
            log_message(LOG_ERROR, "Error waiting for child process: %d\n", errno);
        }
    }

    close(server_socket);

    return error;
}
