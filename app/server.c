#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
// NOTE: on codecrafters exclude this deps
/* #include <bits/pthreadtypes.h> */
#include <pthread.h>
#include <fcntl.h>

#include "constants.h"
#include "http.h"
#include "server.h"

// TODO: build a thread safe pool and a thread-pool to improve performances

// a little hack to avoid malloc a int just to pass a file descriptor
// dir used to serve static files
char* static_serve_dir = "./";

int main(int argc, char *argv[]) {
	// Disable output buffering
	setbuf(stdout, NULL);
 	setbuf(stderr, NULL);

	parse_flags(argc, argv);

	printf("Logs from your program will appear here!\n");

	int server_fd;
	socklen_t client_addr_len;
	struct sockaddr_in client_addr;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}

	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}

	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(4221),
									 .sin_addr = { htonl(INADDR_ANY) },
									};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}

	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}

	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	// create threads detached so I don't need to pthread_wait them
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	while(1) {
		http_arg client_fd;
		client_fd.value = accept(server_fd, (struct sockaddr *) &client_addr, &client_addr_len);
		printf("Client connected\n");

		pthread_t new_thread;
		pthread_create(&new_thread, &attr, serve_http, (void *)client_fd.ptr);
	}
	close(server_fd);

	return 0;
}

void* serve_http(void* arg) {
	http_arg arg_ptr;
	arg_ptr.ptr = (int*) arg;
	int client_fd = arg_ptr.value; // little trick
	char buffer[BUFF_SIZE];
	size_t bytes_read = read(client_fd, buffer, BUFF_SIZE - 1);
	if (bytes_read < BUFF_SIZE - 1) {
		buffer[bytes_read] = '\0';
	}
	request_t* req = NULL;
	if (bytes_read > 0) {
		req = parse_request(buffer, bytes_read);
		debug_request(req);
	}

	if (req) {
		if (req->url[0] == '/' && req->url[1] == '\0') {
			write(client_fd, OK, strlen(OK));
		} else if (!strncmp(req->url, "/echo/", 6)) {
			write(client_fd, OK_HEADER, strlen(OK_HEADER));
			write(client_fd, CONTENT_TEXT, strlen(CONTENT_TEXT));

			char* echo = req->url + 6; // skip 6 character
			write_body(client_fd, echo, strlen(echo));
		} else if (!strncmp(req->url, "/user-agent", 11)) {
			write(client_fd, OK_HEADER, strlen(OK_HEADER));
			write(client_fd, CONTENT_TEXT, strlen(CONTENT_TEXT));
			header_t* user_agent = get_header(req, USER_AGENT);
			write_body(client_fd, user_agent->value, user_agent->value_len);
		} else if (!strncmp(req->url, "/files/", 7)) {
			char* filename = req->url + 7;
			char path[PATH_MAX_LEN];
			size_t index = 0;
			for (; static_serve_dir[index] != '\0'; index++) {
				path[index] = static_serve_dir[index];
			}
			for (size_t i = 0; filename[i] != '\0'; i++) {
				path[index++] = filename[i];
			}
			path[index] = '\0';

			// TODO: add mode_t to complete open params
			if (req->method == GET) {
				int file_fd = open(path, O_RDONLY);
				if (file_fd == -1) {
					write(client_fd, NOT_FOUND, strlen(NOT_FOUND));
				} else {
					write(client_fd, OK_HEADER, strlen(OK_HEADER));
					write(client_fd, CONTENT_OCTET, strlen(CONTENT_OCTET));

					char file_buff[BUFF_SIZE];
					size_t file_size = read(file_fd, file_buff, BUFF_SIZE - 1);
					file_buff[file_size] = '\0';
					write_body(client_fd, file_buff, file_size);
					close(file_fd);
				}
			} else if (req->method == POST) {
				int file_fd = open(path, O_WRONLY | O_CREAT);
				if (file_fd == -1) {
					write(client_fd, SERVER_ERR, strlen(SERVER_ERR));
				} else {
					int written = write(file_fd, req->body, req->body_len);
					if (written == -1) {
						write(client_fd, SERVER_ERR, strlen(SERVER_ERR));
					} else {
						write(client_fd, CREATED, strlen(CREATED));
					}
				}
			} else {
				write(client_fd, METHOD_NOT_ALLOWED, strlen(METHOD_NOT_ALLOWED));
			}
		} else {
			write(client_fd, NOT_FOUND, strlen(NOT_FOUND));
		}
		free_request(req);
	} else {
		write(client_fd, SERVER_ERR, strlen(SERVER_ERR));
	}

	close(client_fd);
	return NULL;
}

void parse_flags(int argc, char* argv[]) {
	for (int i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--directory", 11)) {
			if (i == argc - 1) {
				fprintf(stderr, "--directory flag used wrong\n");
				exit(1);
			}
			static_serve_dir = argv[i + 1];
			break;
		}
	}
}
