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
        printf("%c", raw[*index]);
        *index = *index + 1;
    }
    printf("\n");
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
    strncpy(url, &raw[start], alloc_size - 1);
    url[alloc_size - 1] = '\0';
    return url;
}

static void parse_headers(char* raw, size_t* index, size_t len, request_t* req) {
    char prev = '\0';
    while(1){
        size_t start = *index;
        // parse one header
        while(*index < len) {
            prev = raw[*index];
            *index = *index + 1;
            if (prev == '\r' && raw[*index] == '\n') {
                break;
            }
        }
        if(!strncmp(CRLF, raw+start, 2) && *index - start == 1) {
            // end headers
            *index = *index + 1;
            return;
        }

        if (!req->headers.elements) {
            req->headers.elements = (header_t*) calloc(ARR_DEFAULT_SIZE, sizeof(header_t));
            req->headers.capacity = ARR_DEFAULT_SIZE;
        }

        if (req->headers.count == req->headers.capacity) {
            req->headers.elements = realloc(req->headers.elements, req->headers.capacity << 1 * sizeof(header_t));
            req->headers.capacity <<= 1;
        }

        // whole header is inside [start, *index]
        // 1. split at ':' -> key
        // 2. drop whiltespaces, remove CRLF -> value
        header_t* header = &req->headers.elements[req->headers.count++];
        size_t split_index = start;
        while(raw[split_index] != ':') {
            split_index++;
        }
        header->key = &raw[start];
        header->key_len = split_index - start;
        split_index++;
        skip_whitespace(raw, &split_index, len);
        header->value = &raw[split_index];
        header->value_len = *index - split_index - 1;

        // start on next line
        *index = *index + 1;
    }
}

static void parse_body(char* raw, size_t* index, size_t len, request_t* req) {
    header_t* content_length = get_header(req, "Content-Length");
    req->body = &raw[*index];
    req->body_len = 0;
    if (content_length) {
        for (size_t i = 0; i < content_length->value_len; i++) {
            req->body_len *= 10;
            req->body_len += content_length->value[i] - '0';
        }
    }
    req->body[req->body_len] = '\0';
}

// assume raw is in scope for the whole request (it will be stack allocated)
request_t* parse_request(char* raw, size_t len) {
    size_t index = 0;
    method_t method = parse_method(raw, &index, len);
    if (method == INVALID) {
        return NULL;
    }
    request_t* req = (request_t*) malloc(sizeof(request_t));
    req->headers.elements = NULL;
    req->headers.count = 0;
    req->headers.capacity = 0;
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
    index += 10;
    parse_headers(raw, &index, len, req);
    parse_body(raw, &index, len, req);
    return req;
}

void free_request(request_t* request) {
    if(!request){
        perror(ERR_FREE_NULL_REQUEST);
        exit(1);
    }

    free(request->url);

    if (request->headers.elements) {
        free(request->headers.elements);
        request->headers.count = 0;
        request->headers.capacity = 0;
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

void debug_request(request_t* request) {
    printf("METHOD: %s\n", method_t_to_str(request->method));
    printf("URL: %s\n", request->url);
    printf("HEADERS: {\n");
    if (request->headers.elements) {
        for(size_t i = 0; i < request->headers.count; i++) {
            header_t* h = &request->headers.elements[i];
            printf("\t%.*s: %.*s\n", h->key_len, h->key, h->value_len, h->value);
        }
    }
    printf("}\n");
    printf("BODY: {\n%.*s", (int) request->body_len, request->body);
    printf("}\n");
}

void write_body(int fd, char* body, size_t size) {
    write(fd, CONTENT_LEN, strlen(CONTENT_LEN));
    dprintf(fd, "%lu\r\n\r\n", size);
    dprintf(fd, "%.*s", (int)size, body);
}

// TODO: maybe transfrom headers into an hashmap
header_t* get_header(request_t* request, char* key) {
    for (size_t i = 0; i < request->headers.count; i++) {
        if (!strncmp(key, request->headers.elements[i].key, request->headers.elements[i].key_len)) {
           return &request->headers.elements[i];
        }
    }
    return NULL;
}
