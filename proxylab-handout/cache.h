/*
 * cache.h - prototypes and definitions for cache utilized in proxy.c
 */


#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/*
 * cache as a doublely linked node list
 * when one cache is newly used
 * update the link relationship
 * put it in the most front of the list
 * then the least used one would always be the tail one
 * a sem_t mutex ensures that only one could write in the mean time
 */
typedef struct cache{
  int conn;			        /* is it a buf while connection */
  char buf[MAX_OBJECT_SIZE];            /* buf */
  char uri[MAXLINE];		        /* buf's uri */
  sem_t mutex;				/* sem_t mutex */
  struct cache *next_cache;	        /* next cache */
  struct cache *prev_cache;		/* previous cache */
}cache;


/* initial CACHE_HEAD and a sem_t mutex for the whole linked list */
void init_cache_list(void);
/* insert cache_node as the head */
void insert_cache_head(cache *cache_node);
/* break the node relations of a cache_node, and link the prev and next */
void repair_cache_link(cache *cache_node);
/* write and insert a cache_node to the beginning of cache node list */
cache *write_cache(char *buf, char *uri);
/* after lost connection, turn off flag conn, handle eviction */
void hadnle_after_disconn(cache *cache_node);
/* read cache */
char *read_cache(cache *cache_node);
/* find cache based on host and path, if not exists, return NULL */
cache *find_cache(char *uri);
/* find the tail cache */
cache *find_tail_cache(void);
/* cache size, not the whole cache size but the buf size */
size_t get_cache_size(cache *cache_node);
/* the size of entire cache list, only the buf size included */
size_t get_whole_cache_size(void);
/* delete a cache_node which is in the tail */
void delete_tail_cache(void);
