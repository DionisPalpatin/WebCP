#define _POSIX_C_SOURCE 200112L


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
#include <signal.h>

#include "../inc/server.h"
#include "../inc/myerrors.h"
#include "../inc/handler.h"


int worker_is_working = 1;


int init_server(struct sockaddr_in *server_addr, int *server_socket) {
	log_message(LOG_STEPS, "init_server()\n");

	int error = OK;
	
	// Создание сокета
	if ((*server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_message(LOG_ERROR, "Error init server socket: errno = %s\n", strerror(errno));
		error = SOCKET_ERR;
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

	log_message(LOG_DEBUG, "i: %d, new_conn: %d\n", i, new_conn);

	fds[i].fd = new_conn;
	fds[i].events = POLLIN | POLLOUT;

	client_conns[i].state = READY_FOR_REQUEST;
	client_conns[i].socket = new_conn;
	client_conns[i].resp_buffer.offset = 0;
	client_conns[i].resp_buffer.total = RESPONSE_BLOCK_SIZE_BYTES;

	client_conns[i].resp_buffer.buffer = malloc(client_conns[i].resp_buffer.total * sizeof(char));
	client_conns[i].path = malloc(sizeof(char) * PATH_MAX);
	client_conns[i].req_buffer.buffer = malloc(sizeof(char) * MAX_REQUEST_SIZE_BYTES);

	if (!client_conns[i].resp_buffer.buffer || !client_conns[i].path || client_conns[i].req_buffer.buffer)
		log_message(LOG_ERROR, "Memory allocating error, path = %d, buffer = %d\n", !client_conns[i].path, client_conns[i].req_buffer.buffer);

	log_message(LOG_STEPS, "end add_new_client()\n");
}


void close_client(struct pollfd *fds, connection_t *client_conns, int i) {
	log_message(LOG_STEPS, "close_client()\n");
	
	log_message(LOG_INFO, "Close client's connections for client with fd = %d\n", fds[i].fd);
	
	shutdown(client_conns[i].socket, SHUT_RDWR);
	close(fds[i].fd);

	fds[i].fd = -1;
	client_conns[i].socket = -1;

	free(client_conns[i].resp_buffer.buffer);
	free(client_conns[i].path);
	free(client_conns[i].req_buffer.buffer);

	client_conns[i].resp_buffer.buffer = NULL;
	client_conns[i].path = NULL;
	client_conns[i].req_buffer.buffer = NULL;
}


int accept_new_client(int server_socket, struct pollfd *fds, connection_t *client_conns, int *nfds) {
    log_message(LOG_STEPS, "accept_new_clients()\n");

    int error = OK;
    int new_conn = accept(server_socket, NULL, NULL);
	log_message(LOG_DEBUG, "new_conn: %d\n", new_conn);

    if (new_conn < 0) {
		log_message(LOG_ERROR, "Error accepting new client: %s\n", strerror(errno));
        return error;
    }

    if (*nfds > MAX_CLIENTS) {
        log_message(LOG_WARNING, "Maximum number of clients reached, refusing new connection\n");
        close(new_conn);
        return error;
    }

    add_new_client(fds, client_conns, *nfds, new_conn);
    (*nfds)++;

    log_message(LOG_INFO, "New client (number %d) is connected\n", *nfds);

    return error;
}


void compress_arrays(struct pollfd *fds, connection_t *conns, int *nfds) {
	int i = 0;
	while (i < *nfds) {
		if (fds[i].fd == -1) {
			for (int j = i; j < *nfds - 1; j++) {
				fds[j] = fds[j + 1];
				conns[j] = conns[j + 1];
			} 
			(*nfds)--;
		} else {
			i++;
		}
	}		
}


void signal_handler(int signum, siginfo_t *info, void *context) {
	int pid = getpid();
	log_message(LOG_INFO, "Process with PID = %d is stopping...\n", pid);

	// Параметры не используются мной, но нужны в соответствии с определения поля структуры struct sigaction.
	// Отключать -Werror не хочу, поэтому делаю так, чтобы компилятор не ругался на неиспользуемые переменные.
	log_message(LOG_INFO, "Params in signal_handler: signum = %d, siginfo = %d, context = %d\n", signum, info == NULL, context == NULL);

	worker_is_working = 0;

	log_message(LOG_INFO, "Process with PID = %d has stopped successfully\n", pid);
}


int handle_sigint(struct sigaction *sa) {
	sa->sa_flags = SA_SIGINFO;
	sa->sa_sigaction = signal_handler;
	sa->sa_flags = 0;
    sigemptyset(&sa->sa_mask);
	
	if (sigaction(SIGINT, sa, NULL) == -1) {
		log_message(LOG_ERROR, "Error while sigaction: %s\n", strerror(errno));
		return SIGACTION_ERR;
	}

	return OK;
}


int worker(int server_socket) {
	log_message(LOG_STEPS, "worker()\n");

	int error = OK;

    // struct pollfd fds[MAX_CLIENTS];
    // connection_t client_connections[MAX_CLIENTS];
	struct pollfd *fds = malloc((MAX_CLIENTS + 1) * sizeof(struct pollfd));
    connection_t *client_connections = malloc((MAX_CLIENTS + 1) * sizeof(connection_t));
    int nfds = 1;

	fds[0].fd = server_socket;
	fds[0].events = POLLIN;
	for (int i = 1; i < MAX_CLIENTS; i++)
		fds[i].fd = -1;

	log_message(LOG_INFO, "Poll initialized successfully\n");

	while (!error) {
		int new_conn = poll(fds, nfds, TIMEOUT);

		if (!worker_is_working)
			break;

		if (new_conn < 0) {
			log_message(LOG_ERROR, "poll() error in worker(): %s\n", strerror(errno));
			error = FORK_ERR;
			continue;
		} else if (new_conn == 0) {
			log_message(LOG_INFO, "poll() timed out. End program.\n");
			error = POLL_TIMEOUT_ERR;
			continue;
		}

		if (fds[0].revents != 0)
				error = accept_new_client(server_socket, fds, client_connections, &nfds);
				
		for (int i = 1; i < nfds; i++) {
			if (fds[i].revents == 0)
				continue;

			log_message(LOG_DEBUG, "for cycle begin. i: %d, socket: %d\n", i, client_connections[i].socket);	
			log_message(LOG_DEBUG, "State before handle_client(), state: %d, offset: %d, total: %d\n",
				client_connections[i].state, client_connections[i].resp_buffer.offset, client_connections[i - 1].resp_buffer.total);

			handle_client(&client_connections[i]);

			log_message(LOG_DEBUG, "State after handle_client(), state: %d, offset: %d, total: %d\n",
				client_connections[i].state, client_connections[i].resp_buffer.offset, client_connections[i - 1].resp_buffer.total);

			if (client_connections[i].state == COMPLETE || client_connections[i].state == ERROR)
				close_client(fds, client_connections, i);
		}

		compress_arrays(fds, client_connections, &nfds);
	}

	log_message(LOG_ERROR, "Error after all: %d", error);

	log_message(LOG_INFO, "Clean memory after work\n");
	for (int i = 0; i < MAX_CLIENTS + 1; i++)
		close_client(fds, client_connections, i);

	free(fds);
	free(client_connections);	

	return error;
}


int server(void) {
    int server_socket;
    struct sockaddr_in server_addr;

	int error = open_logs_files();
	if (error)
		return error;

	struct sigaction sa;
	error = handle_sigint(&sa);
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
