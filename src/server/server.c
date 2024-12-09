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

#include "../inc/server.h"
#include "../inc/myerrors.h"


int init_server(struct sockaddr_in *server_addr, int *server_socket) {
	int error = OK;
	
	// Создание сокета
	if ((*server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		log_message(LOG_ERROR, "Error init server socket\n");
		error = SOCKET_ERR;
	}

	int enable = 1;
	if (!error && setsockopt(*server_socket, SOL_SOCKET,  SO_REUSEADDR, (char *)&enable, sizeof(enable)) < 0)
	{
		log_message(LOG_ERROR, "Error while setsockopt() in init_server()\n");
		error = SOCK_OPTS_ERR;
	}

	int flags = fcntl(*server_socket, F_GETFL, 0);
	if (!error && fcntl(*server_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
		log_message(LOG_ERROR, "Error while set nonblocking status for server socket in init_server()\n");
		error = SET_NONBLOCKING_ERR;
	}

	// Установка параметров сервера
	server_addr->sin_family = AF_INET;
	server_addr->sin_addr.s_addr = INADDR_ANY;
	server_addr->sin_port = htons(PORT);

	// Привязка сокета
	if (!error && bind(*server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
		log_message(LOG_ERROR, "Error while binding server socket\n");
		error = BIND_SOCKET_ERR;
	}

	// Прослушивание входящих подключений
	if (!error && listen(server_socket, SOMAXCONN) == -1) {
		log_message(LOG_ERROR, "Error init server listening\n");
		error = LISTEN_ERR;
	}

	log_message(LOG_INFO, "Server was started successfully\n");
	log_message(LOG_INFO, "Server is listening on port %d\n", PORT);

	return error;
}


int accept_new_clients(int server_socket, struct pollfd *fds, int *nfds) {
	int cur_clients = *nfds;
	int error = OK;

	int new_conn = accept(server_socket, NULL, NULL);
	while (new_conn > 0) {
		if (cur_clients >= MAX_CLIENTS) {
			log_message(LOG_WARNING, "Maximum number of clients reached, refusing new connection\n");
			break;
		}

		fds[cur_clients].fd = new_conn;
		fds[cur_clients].events = POLLIN | POLLOUT;
		cur_clients++;

		log_message(LOG_INFO, "New client (number %d) is connected\n", cur_clients);

		new_conn = accept(server_socket, NULL, NULL);
	}

	if (new_conn == -1) {
		log_message(LOG_ERROR, "Error accepting new client\n");
		error = ACCEPT_ERR;
	}


	log_message(LOG_INFO, "Total new clients were connected: %d\n", cur_clients - *nfds);
	*nfds = cur_clients;

	return error;
}


int worker(struct sockaddr_in *server_addr, int server_socket) {
	int error = OK;

	struct pollfd fds[MAX_CLIENTS + 1];
	int nfds = 1;  // Пока что один сокет, и тот серверный

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

	close(server_socket);

	return error;
}