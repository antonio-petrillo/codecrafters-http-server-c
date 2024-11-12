#include "http.h"
#include "constants.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>

static int is_whitespace(char c) { // don't skip \r\n
    return c == ' ' || c == '\t';
}

static method_t parse_method(char* raw, size_t* index, size_t len) {
    method_t method = INVALID;
    size_t start = *index;
    while(*index < len && raw[*index] != '\0' && !is_whitespace(raw[*index])) {
       *index = *index + 1;
    }
    char* verify = NULL;
    size_t verify_len = 0;
    switch(raw[start]) {
        case 'G':
            method = GET;
            verify = "GET";
            verify_len = 3;
            break;
        case 'P':
            verify = "GET";
            if (raw[start + 1] == 'O') {
                method = POST;
                verify = "POST";
                verify_len = 4;
            } else {
                method = PUT;
                verify = "PUT";
                verify_len = 3;
            }
            break;
        case 'D':
            method = DELETE;
            verify = "DELETE";
            verify_len = 6;
            break;
    }
    if (verify_len > 0 && strncmp(raw, verify, verify_len)) {
       method = INVALID;
    }
    return method;
}

static void skip_whitespace(char* raw, size_t* index, size_t len) {
    while(*index < len && raw[*index] != '\0') {
        if (!is_whitespace(raw[*index])) {
            return;
        }
        *index = *index + 1;
    }
}

static char* parse_url(char* raw, size_t* index, size_t len) {
    size_t start = *index;
    while (!is_whitespace(raw[*index])) {
        *index = *index + 1;
    }

    if (*index == len || *index == start) { // missing \r\n invalid request
        return NULL;
    }
    size_t alloc_size = *index - start + 1;
    char* url = calloc(alloc_size, sizeof(char*));
    if (!url) {
        return NULL;
    }
    strncpy(url, &raw[start], alloc_size - 1);
    url[alloc_size - 1] = '\0';
    return url;
}

// assume raw is in scope for the whole request (it will be stack allocated)
request_t* parse_request(char* raw, size_t len) {
    size_t index = 0;
    method_t method = parse_method(raw, &index, len);
    if (method == INVALID) {
        return NULL;
    }
    request_t* req = (request_t*) malloc(sizeof(request_t));
    if (!req) {
        return NULL;
    }
    req->method = method;
    skip_whitespace(raw, &index, len);
    req->url = parse_url(raw, &index, len);
    if (!req->url) {
        free(req);
        return NULL;
    }
    skip_whitespace(raw, &index, len);
    if(strncmp("HTTP/1.1\r\n", &raw[index], 10)) {
        free(req);
        return NULL;
    }
    return req;
}

void free_request(request_t* request) {
    if(!request){
        perror(ERR_FREE_NULL_REQUEST);
        exit(1);
    }

    free(request->url);

    if (request->headers.count > 0) {
        free(request->headers.elements);
    }

    if (request->body_len > 0) {
        free(request->body);
    }

    free(request);
}

static char* method_t_to_str(method_t m) {
    switch (m) {
        case GET:
            return "GET";
        case POST:
            return "POST";
        case PUT:
            return "PUT";
        case DELETE:
            return "DELETE";
        default:
            break;
    }
    return "INVALID";
}

void debub_request(request_t* request) {
    printf("METHOD := %s\n", method_t_to_str(request->method));
    printf("URL := %s\n", request->url);
}

void write_body(int fd, char* body, size_t size) {
    write(fd, CONTENT_LEN, strlen(CONTENT_LEN));
    dprintf(fd, "%lu\r\n\r\n", size);
    dprintf(fd, "%.*s", (int)size, body);
}
