#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "cache.h"

#define MAX_URL_LEN 2048
#define MAX_STR_LEN 8192

typedef struct {
  char hostname[MAX_URL_LEN];
  char port[16];
  char uri[MAX_URL_LEN];
} url_t;

typedef struct {
  char method[64];
  url_t *url;
  char version[64];
} request_line_t;

typedef struct {
  int client_fd;
  cache_t *global_cache;
} req_thread_arg_t;

url_t *parse_url(char* url);
request_line_t *parse_request_line(char* request);
void request_handler(void *vargv);
void *request_thread(void *vargv);