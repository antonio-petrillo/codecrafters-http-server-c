#ifndef SERVER_H_
#define SERVER_H_

typedef union {
	int* ptr;
	int value;
} http_arg;

void* serve_http(void* arg);

void parse_flags(int argc, char* argv[]);


#endif // SERVER_H_
