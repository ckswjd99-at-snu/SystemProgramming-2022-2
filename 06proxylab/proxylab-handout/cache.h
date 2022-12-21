#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "csapp.h"

#define MAX_HOST_LEN 1024

typedef struct cache_obj {
  char host[MAX_HOST_LEN];
  char uri[MAX_HOST_LEN];
  char *header;
  char *data;
  int data_size;
  time_t used_at;
  struct cache_obj *prev;
  struct cache_obj *next;
} cache_obj_t;

typedef struct {
  int max_size;
  int max_obj_size;
  int now_size;
  sem_t mutex;
  cache_obj_t *head;
} cache_t;

cache_t *new_cache(int max_size, int max_obj_size);
cache_obj_t *find_cache(cache_t *cache, char *host, char *uri);
void free_cache(cache_t *cache);
void print_cache(cache_t *cache);
cache_obj_t *new_object(char *host, char *uri, char *header, char *data, int data_size);
int push_front(cache_t *cache, cache_obj_t *obj);
cache_obj_t *pop_object(cache_t *cache, cache_obj_t *obj);
void free_object(cache_obj_t *obj);
void P_cache(cache_t *cache);
void V_cache(cache_t *cache);