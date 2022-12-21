#include "handler.h"

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

url_t *parse_url(char* url) {
  url_t *result = malloc(sizeof(url_t));

  char *temp;
  char *idx_start = url;
  char *idx_end = url + strlen(url);
  
  /* PARSE HOSTNAME */
  if ((temp = strstr(idx_start, "://")) != NULL)
    idx_start = temp + 3;
  
  if ((temp = strstr(idx_start, "/")) != NULL)
    idx_end = temp;
  if ((temp = strstr(idx_start, "?")) != NULL && temp < idx_end)
    idx_end = temp;
  if ((temp = strstr(idx_start, "#")) != NULL && temp < idx_end)
    idx_end = temp;
  if ((temp = strstr(idx_start, ":")) != NULL && temp < idx_end)
    idx_end = temp;
  
  strncpy(result->hostname, idx_start, (int)(idx_end - idx_start));
  result->hostname[(int)(idx_end - idx_start)] = '\0';

  /* PARSE PORT */
  if (temp != NULL) {
    idx_start = temp + 1;
    
    idx_end = url + strlen(url);
    if ((temp = strstr(idx_start, "/")) != NULL && temp < idx_end)
      idx_end = temp;
    if ((temp = strstr(idx_start, "?")) != NULL && temp < idx_end)
      idx_end = temp;
    if ((temp = strstr(idx_start, "#")) != NULL && temp < idx_end)
      idx_end = temp;
    
    strncpy(result->port, idx_start, idx_end - idx_start);
    result->port[(int)(idx_end - idx_start)] = '\0';
  }
  else {
    *(result->port) = '\0';
  }

  /* PARSE URI */
  idx_start = url;
  if ((temp = strstr(idx_start, "://")) != NULL)
    idx_start = temp + 3;
  
  if ((temp = strstr(idx_start, "/")) != NULL) {
    idx_start = temp;

    idx_end = url + strlen(url);
    if ((temp = strstr(idx_start, "#")) != NULL && temp < idx_end)
      idx_end = temp;
    
    strncpy(result->uri, idx_start, idx_end - idx_start);
    result->uri[(int)(idx_end - idx_start)] = '\0';
  }
  else {
    strncpy(result->uri, "/\0", 2);
  }
  
  return result;
}

request_line_t *parse_request_line(char* request) {
  request_line_t *result = malloc(sizeof(request_line_t));
  char url[MAX_URL_LEN];
  sscanf(request, "%s %s %s", result->method, url, result->version);
  result->url = parse_url(url);

  return result;
}

void request_handler(void *vargv) {
  /* PARSE ARGS */
  req_thread_arg_t *req_args = (req_thread_arg_t *)vargv;
  int client_fd = req_args->client_fd;
  cache_t *cache = req_args->global_cache;

  char temp_buf[MAX_STR_LEN];
  char client_buf[MAX_STR_LEN];
  request_line_t *request_line;

  /* READ REQUEST*/
  size_t read_num;
  rio_t client_rio;
  Rio_readinitb(&client_rio, client_fd);

  if ((read_num = Rio_readlineb(&client_rio, client_buf, MAX_STR_LEN)) == 0) {
    printf("Not readable request!\n");
    return;
  }

  /* PARSE CLIENT REQUEST LINE */
  request_line = parse_request_line(client_buf);
  if (strcmp(request_line->method, "GET")) {
    printf("This proxy only accepts GET method!\n");
    free(request_line->url);
    free(request_line);
    return;
  }

  /* CHECK CACHE */
  P_cache(cache);
  cache_obj_t *hit_obj;
  if ((hit_obj = find_cache(cache, request_line->url->hostname, request_line->url->uri)) != NULL) {
    // cache hit! send object
    Rio_writen(client_fd, hit_obj->data, hit_obj->data_size);

    // before return
    V_cache(cache);
    free(request_line->url);
    free(request_line);
    
    return;
  }
  V_cache(cache);

  /* SEND SERVER REQUEST LINE */
  int server_fd;
  rio_t server_rio;
  char server_buf[MAX_STR_LEN];

  server_fd = Open_clientfd(request_line->url->hostname, request_line->url->port);
  Rio_readinitb(&server_rio, server_fd);

  sprintf(server_buf, "GET %s HTTP/1.0\r\n", request_line->url->uri);
  Rio_writen(server_fd, server_buf, strlen(server_buf));

  /* SEND SERVER CUSTOM HEADERS */
  sprintf(
    server_buf, 
    "Host: %s\r\nConnection: close\r\nProxy-Connection: close\r\n%s", 
    request_line->url->hostname, user_agent_hdr
  );
  Rio_writen(server_fd, server_buf, strlen(server_buf));

  /* FORWARD HEADERS */
  while ((read_num = Rio_readlineb(&client_rio, client_buf, MAX_STR_LEN)) > 0) {
    sscanf(client_buf, "%[^:]:", temp_buf);

    if (
      strcmp(temp_buf, "Host") &&
      strcmp(temp_buf, "Connection") &&
      strcmp(temp_buf, "Proxy-Connection") &&
      strcmp(temp_buf, "User-Agent")
    ) Rio_writen(server_fd, client_buf, strlen(client_buf));

    if (strcmp(client_buf, "\r\n") == 0) break;
  }

  /* FORWARD RESPONSE */
  size_t cache_len = 0;
  char cache_header[MAX_STR_LEN] = "";
  char *cache_data = malloc(sizeof(char) * cache->max_obj_size);

  while ((read_num = Rio_readnb(&server_rio, server_buf, MAX_STR_LEN)) > 0) {
    if (cache_len + read_num < cache->max_obj_size)
      memcpy(cache_data + cache_len, server_buf, read_num);
    Rio_writen(client_fd, server_buf, read_num);
    cache_len += read_num;
  }

  /* PUSH CACHE */
  if (cache_len < cache->max_obj_size) {
    P_cache(cache);

    cache_obj_t *obj = new_object(
      request_line->url->hostname,
      request_line->url->uri,
      cache_header, cache_data, cache_len
    );

    if (push_front(cache, obj) != 0)
      free_object(obj);

    V_cache(cache);
  }

  /* MEMORY RESTORE */
  free(cache_data);
  free(request_line->url);
  free(request_line);

  return;
}

void *request_thread(void *vargv) {
  /* RUN DETACHED */
  Pthread_detach(pthread_self());

  /* RUN HANDLER */
  request_handler(vargv);

  return NULL;
}
