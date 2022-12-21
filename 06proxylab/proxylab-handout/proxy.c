#include <stdio.h>
#include <stdlib.h>
#include "csapp.h"
#include "handler.h"

/* Recommended max cache sizes */
#define MAX_CACHE_SIZE  1049000
#define MAX_OBJ_SIZE    102400

/* Cache */
static cache_t *global_cache;

int main(int argc, char *argv[])
{
    char *port = argv[1];
    pthread_t pid;

    global_cache = new_cache(MAX_CACHE_SIZE, MAX_OBJ_SIZE);
    
    int proxy_fd = Open_listenfd(port);
    struct sockaddr proxy_addr;

    while (1) {
        socklen_t addr_length = sizeof(proxy_addr);
        int client_fd = Accept(proxy_fd, &proxy_addr, &addr_length);

        req_thread_arg_t *vargv = malloc(sizeof(req_thread_arg_t));
        vargv->client_fd = client_fd;
        vargv->global_cache = global_cache;

        Pthread_create(&pid, NULL, request_thread, (void *)vargv);
    }

    free_cache(global_cache);
    
    return 0;
}
