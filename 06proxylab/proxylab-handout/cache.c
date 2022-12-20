#include "cache.h"

cache_t *new_cache(int max_size, int max_obj_size) {
  cache_t *result = malloc(sizeof(cache_t));

  result->max_size = max_size;
  result->max_obj_size = max_obj_size;
  result->now_size = 0;
  result->head = NULL;

  return result;
}

cache_obj_t *find_cache(cache_t *cache, char *host, char *uri) {
  cache_obj_t *iter_obj = cache->head, *temp;

  /* TRAVERSE OBJ LIST */
  while (iter_obj != NULL) {
    temp = iter_obj;
    iter_obj = iter_obj->next;

    if (!strcmp(temp->host, host) && !strcmp(temp->uri, uri)) {
      iter_obj = temp;
      break;
    }
  }

  /* UPDATE USED LOG */
  if (iter_obj != NULL) {
    temp = pop_object(cache, iter_obj);
    time(&(temp->used_at));
    push_front(cache, temp);
  }

  return iter_obj;
}

void free_cache(cache_t *cache) {
  cache_obj_t *iter_obj = cache->head, *temp;

  while (iter_obj != NULL) {
    temp = iter_obj;
    iter_obj = iter_obj->next;

    free_object(iter_obj);
  }
}

void print_cache(cache_t *cache) {
  cache_obj_t *iter_obj = cache->head, *temp;

  printf("max_size: %d, max_obj_size: %d, now_size: %d\n", cache->max_size, cache->max_obj_size, cache->now_size);

  printf("%-20s\t%-20s\t%-20s\t%-10s\t%-50s\n", "[timestamp]", "[host]", "[uri]", "[size]", "[data]");
  while (iter_obj != NULL) {
    temp = iter_obj;
    iter_obj = iter_obj->next;

    printf("%-20ld\t%-20s\t%-20s\t%-10d\t%-50s\n", temp->used_at, temp->host, temp->uri, temp->data_size, temp->data);
  }
  printf("\n");
}

cache_obj_t *new_object(char *host, char *uri, char *header, char *data) {
  cache_obj_t *created = malloc(sizeof(cache_obj_t));
  created->header = malloc(sizeof(char) * (strlen(header)+1));
  created->data = malloc(sizeof(char) * (strlen(data)+1));
  created->data_size = sizeof(char) * (strlen(data)+1);
  time(&(created->used_at));

  strcpy(created->host, host);
  strcpy(created->uri, uri);
  strcpy(created->header, header);
  strcpy(created->data, data);

  return created;
}

void push_front(cache_t *cache, cache_obj_t *obj) {
  /* CHECK OBJ SIZE */
  if (cache->max_obj_size < obj->data_size) {
    return;
  }

  /* CHECK AND CREATE CACHE SPACE */
  while (cache->max_size - cache->now_size < obj->data_size) {
    cache_obj_t *last = cache->head;
    while (last != NULL && last->next != NULL) last = last->next;

    free_object(pop_object(cache, last));
  }

  if (cache->head == NULL) {
    obj->prev = NULL;
    obj->next = NULL;
    cache->head = obj;
  }
  else {
    obj->prev = NULL;
    obj->next = cache->head;
    obj->next->prev = obj;
    cache->head = obj;
  }
  cache->now_size += obj->data_size;
}

cache_obj_t *pop_object(cache_t *cache, cache_obj_t *obj) {
  if (obj->prev == NULL && obj->next == NULL) {
    // only object in cache
    cache->head = NULL;
    obj->prev = NULL;
    obj->next = NULL;
  }
  else if (obj->prev == NULL) {
    // head object in cache
    cache->head = obj->next;
    cache->head->prev = NULL;
    obj->prev = NULL;
    obj->next = NULL;
  }
  else if (obj->next == NULL) {
    // tail object in cache
    obj->prev->next = NULL;
    obj->prev = NULL;
    obj->next = NULL;
  }
  else  {
    // middle object in cache
    obj->prev->next = obj->next;
    obj->next->prev = obj->prev;
    obj->prev = NULL;
    obj->next = NULL;
  }

  cache->now_size -= obj->data_size;
  return obj;
}

void free_object(cache_obj_t *obj) {
  free(obj->data);
  free(obj->header);
  free(obj);
}

/* MAIN FOR TEST */
// int main () {
//   cache_t *cache_test = new_cache(100, 30);

//   push_front(cache_test, new_object("5iq.cc", "/index1.html", "", "<html>hello world1!</html>"));
//   push_front(cache_test, new_object("5iq.cc", "/index2.html", "", "<html>hello world2!</html>"));
//   push_front(cache_test, new_object("5iq.cc", "/index3.html", "", "<html>hello world3!</html>"));
//   push_front(cache_test, new_object("5iq.cc", "/index4.html", "", "<html>hello world4!</html>"));

//   print_cache(cache_test);
  
//   find_cache(cache_test, "5iq.cc", "/index1.html");
//   find_cache(cache_test, "5iq.cc", "/index2.html");
  
//   print_cache(cache_test);

//   push_front(cache_test, new_object("5iq.cc", "/index5.html", "", "<html>hello world5!</html>"));
//   push_front(cache_test, new_object("5iq.cc", "/index6.html", "", "<html>hello world6!</html>"));
//   push_front(cache_test, new_object("5iq.cc", "/index7.html", "", "<html>hello world7!</html>"));
//   find_cache(cache_test, "5iq.cc", "/index4.html");
//   find_cache(cache_test, "5iq.cc", "/index5.html");

//   push_front(cache_test, new_object("5iq.cc", "/index4.html", "", "<html>hello world4!</html>"));
//   // push_front(cache_test, new_object("5iq.cc", "/index4.html", "", "<html>hello world4!</html>"));
//   // push_front(cache_test, new_object("5iq.cc", "/index4.html", "", "<html>hello world4!</html>"));
//   // push_front(cache_test, new_object("5iq.cc", "/index4.html", "", "<html>hello world4!</html>"));

//   print_cache(cache_test);
// }