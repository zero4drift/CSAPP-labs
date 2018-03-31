#include "csapp.h"
#include "cache.h"
#include <stdio.h>

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:52.0) Gecko/20100101 Firefox/52.0\r\n";
static const char *accept_hdr = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n";
static const char *accept_encoding_hdr = "Accept-Encoding: gzip, deflate\r\n";
static const char *language_hdr = "Accept-Language: zh,en-US;q=0.7,en;q=0.3\r\n";
static const char *connection_hdr = "Connection: close\r\n";
static const char *pxy_connection_hdr = "Proxy-Connection: close\r\n";
static const char *secure_hdr = "Upgrade-Insecure-Requests: 1\r\n";

void *thread(void *var);
void sigpipe_handler(int sig);
void doit(int fd);
int parse_uri(char *uri, char *host, char *port, char *path);
void read_requesthdrs(rio_t *rp);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);


int main(int argc, char **argv) 
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t t_id;

  init_cache_list();

  /* Check command line args */
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  Signal(SIGPIPE, sigpipe_handler);
  
  listenfd = Open_listenfd(argv[1]);
  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, 
		port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    Pthread_create(&t_id, NULL, thread, &connfd);
  }
}

/* Main working function */
void doit(int fd) 
{
  char buf[MAXLINE], up_buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], path[MAXLINE], port[MAXLINE];
  char cache[MAX_OBJECT_SIZE];
  rio_t rio, sio;
  int up_fd, content_type_ignore;
  size_t n, m = 0;
  struct cache *cache_node;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET"))
    {
      clienterror(fd, method, "501", "Not Implemented",
		  "Proxy does not implement this method");
      return;
    }
  read_requesthdrs(&rio);
  if(!parse_uri(uri, host, port, path))
    {
      clienterror(fd, uri, "404", "Not found", "Request could not be parsed");
      return;
    }

  /* if already cached */
  cache_node = find_cache(uri);
  if(cache_node)
    {
      strcpy(up_buf, read_cache(cache_node));
      Rio_writen(fd, up_buf, strlen(up_buf));
      return;
    }
  /* if not cached */
  up_fd = Open_clientfd(host, port);
  Rio_readinitb(&sio, up_fd);

  sprintf(up_buf, "GET %s HTTP/1.0\r\nHost: %s\r\n%s%s%s%s%s%s%s\r\n", path, host, user_agent_hdr, accept_hdr, language_hdr, accept_encoding_hdr, connection_hdr, secure_hdr, pxy_connection_hdr);
  Rio_writen(up_fd, up_buf, strlen(up_buf));

  while((n = Rio_readlineb(&sio, up_buf, MAXLINE)) != 0)
    {
      content_type_ignore = m;
      m += n;
      Rio_writen(fd, up_buf, n);
      if(content_type_ignore) strcat(cache, up_buf);
    }
  if(m <= MAX_OBJECT_SIZE) cache_node = write_cache(cache, uri);
  /* ready to disconnect */
  hadnle_after_disconn(cache_node);
}

/* 
 * This parse function I just copy the idea from another genius
 * Do not want to spend too much time in that helper function
 */
int parse_uri(char *uri, char *host, char *port, char *path) 
{
  const char *ptr;
  const char *tmp;
  char scheme[10];
  int len;
  int i;

  ptr = uri;
  
  tmp = strchr(ptr, ':');
  if (NULL == tmp) 
    return 0;
    
  len = tmp - ptr;
  (void)strncpy(scheme, ptr, len);
  scheme[len] = '\0';
  for (i = 0; i < len; i++)
    scheme[i] = tolower(scheme[i]);
  if (strcasecmp(scheme, "http"))
    return 0;
  tmp++;
  ptr = tmp;

  for ( i = 0; i < 2; i++ ) {
    if ( '/' != *ptr ) {
      return 0;
    }
    ptr++;
  }

  tmp = ptr;
  while ('\0' != *tmp) {
    if (':' == *tmp || '/' == *tmp)
      break;
    tmp++;
  }
  len = tmp - ptr;
  (void)strncpy(host, ptr, len);
  host[len] = '\0';

  ptr = tmp;
  
  if (':' == *ptr) {
    ptr++;
    tmp = ptr;
    /* Read port */
    while ('\0' != *tmp && '/' != *tmp)
      tmp++;
    len = tmp - ptr;
    (void)strncpy(port, ptr, len);
    port[len] = '\0';
    ptr = tmp;
  } else {
    port = "80";
  }
  
  if ('\0' == *ptr) {
    strcpy(path, "/");
    return 1;
  }

  tmp = ptr;
  while ('\0' != *tmp)
    tmp++;
  len = tmp - ptr;
  (void)strncpy(path, ptr, len);
  path[len] = '\0';

  return 1;
}

/* Exactly same as the one in tiny.c */
void read_requesthdrs(rio_t *rp) 
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/* Exactly same as the one in tiny.c */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

/* Handle SIGPIPE signal */
void sigpipe_handler(int sig)
{
  printf("SIGPIPE handled %d\n", sig);
  return;
}

/* thread */
void *thread(void *var)
{
  int fd = *((int *)var);
  Pthread_detach(Pthread_self());
  Signal(SIGPIPE, sigpipe_handler);
  doit(fd);
  Close(fd);
  return NULL;
}
