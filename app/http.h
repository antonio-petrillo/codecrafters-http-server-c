#ifndef HTTP_H_
#define HTTP_H_

#include <stdlib.h>

typedef enum {
  GET,
  POST,
  PUT,
  DELETE,
  INVALID,
} method_t;

typedef struct header {
    char* key;
    char* value;
} header_t;

typedef struct headers {
    size_t count;
    size_t capacity;
    header_t* elements;
} headers_t;

typedef struct {
    method_t method;
    char* url;
    headers_t headers;
    char* body;
    size_t body_len;
} request_t;

request_t* parse_request(char* raw, size_t len);
void free_request(request_t* request);

#endif // HTTP_H_
