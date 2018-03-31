#include "cache.h"

/* Global variable */
cache *CACHE_HEAD;
sem_t mutex;

/* initial CACHE_HEAD and sem_t mutex for the whole linked list */
void init_cache_list(void)
{
  CACHE_HEAD = NULL;
  Sem_init(&mutex, 0, 1);
}
/* insert cachenode as the head */
void insert_cache_head(cache *cache_node)
{
  if(CACHE_HEAD == cache_node) return;
  else if(CACHE_HEAD)
    {
      CACHE_HEAD->prev_cache = cache_node;
      cache_node->next_cache = CACHE_HEAD;
      cache_node->prev_cache = NULL;
      CACHE_HEAD = cache_node;
    }
  else CACHE_HEAD = cache_node;
}

/* break the node relations of a cache_node, and link the prev and next */
void repair_cache_link(cache *cache_node)
{
  cache *prev, *next;
  if(cache_node == CACHE_HEAD) return;
  else
    {
      prev = cache_node->prev_cache;
      next = cache_node->next_cache;
      if(prev) prev->next_cache = next;
      if(next) next->prev_cache = prev;
      cache_node->prev_cache = NULL;
      cache_node->next_cache = NULL;
    }
}

/* write cache */
cache *write_cache(char *buf, char *uri)
{
  P(&mutex);
  cache *cache_node = find_cache(uri);
  if(!cache_node)
    {
      cache_node = (cache *)malloc(sizeof(cache));
      Sem_init(&(cache_node->mutex), 0, 1);
    }
  P(&(cache_node->mutex));
  cache_node->conn = 1;		/* write as connection */
  strcpy(cache_node->uri, uri);
  strcpy(cache_node->buf, buf);
  V(&(cache_node->mutex));
  if(!(cache_node == CACHE_HEAD))
    {
      repair_cache_link(cache_node);
      insert_cache_head(cache_node);
    }
  V(&mutex);
  return cache_node;
}

/* after lost connection, turn off flag conn, handle eviction */
void hadnle_after_disconn(cache *cache_node)
{
  size_t size = 0;
  P(&(cache_node->mutex));
  cache_node->conn = 0;
  V(&(cache_node->mutex));
  P(&mutex);
  size = get_whole_cache_size();
  while(size > MAX_CACHE_SIZE)
    {
      printf("1");
      delete_tail_cache();
      size = get_whole_cache_size();
    }
  V(&mutex);
}

/* read cache */
char *read_cache(cache *cache_node)
{
  if(!(cache_node == CACHE_HEAD))
    {
      P(&mutex);
      repair_cache_link(cache_node);
      insert_cache_head(cache_node);
      V(&mutex);
    }
  return cache_node->buf;
}

/* find cache based on host and path, if not exists, return NULL */
cache *find_cache(char *uri)
{
  cache *cache_node = CACHE_HEAD;
  while(cache_node)
    {
      printf("2");
      if(!strcmp(cache_node->uri, uri))
	return cache_node;
      else cache_node = cache_node->next_cache;
    }
  return NULL;
}

/* cache size, not the whole cache size but the buf size */
size_t get_cache_size(cache *cache_node)
{
  return strlen(cache_node->buf);
}

/* the size of entire cache list, only the buf size included */
size_t get_whole_cache_size(void)
{
  cache *cache_node = CACHE_HEAD;
  size_t size = 0;

  while(cache_node)
    {
      printf("3");
      if(!cache_node->conn) size += get_cache_size(cache_node);
      cache_node = cache_node->next_cache;
    }

  return size;
}

/* find tail cache */
cache *find_tail_cache(void)
{
  cache *temp_cache = NULL;
  cache *tail_cache = CACHE_HEAD;

  while(tail_cache)
    {
      printf("4");
      if(!tail_cache->next_cache && !tail_cache->conn) return tail_cache;
      else if(!tail_cache->conn) temp_cache = tail_cache;
      tail_cache = tail_cache->next_cache;
    }
  return temp_cache;
}

/* delete a tail cache_node */
void delete_tail_cache(void)
{
  cache *tail_cache = find_tail_cache();
  if(tail_cache)
    {
      repair_cache_link(tail_cache);
      free(tail_cache);
    }
}
