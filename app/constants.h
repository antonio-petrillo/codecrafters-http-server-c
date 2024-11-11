#ifndef CONSTANTS_H_
#define CONSTANTS_H_

#define HTTP_VERSION "HTTP/1.1"
#define CRLF "\r\n\r\n"
#define BUFF_SIZE 4096

#define ERR_FREE_NULL_REQUEST "Error trying to free NULL request\n"

#define OK "HTTP/1.1 200 OK\r\n\r\n"
#define NOT_OK "HTTP/1.1 404 Not Found\r\n\r\n"
#define SERVER_ERR "HTTP/1.1 500 Internal Server Error\r\n\r\n"

#endif // CONSTANTS_H_
