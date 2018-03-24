/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * Based on an explicit, double linked free list structure;
 * and a free strategy LIFO is applied to increase the performance.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name */
  "self-study",
  /* First member's full name */
  "zero4drift",
  /* First member's email address */
  "xxx@yyy.com",
  /* Second member's full name (leave blank if none) */
  "",
  /* Second member's email address (leave blank if none) */
  ""
};

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

/* Doubly linked free list manipulations */
#define GET_PREV(p)      (GET(p))	/* Just a alias of former GET */
#define PUT_PREV(p, val) (PUT(p, (unsigned int)val))  /* Alias of former PUT */
#define GET_SUCC(p)      (*((unsigned int *)p + 1))
#define PUT_SUCC(p, val) (*((unsigned int *)p + 1) = (unsigned int)(val))

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
/* 
   A pointer whose value is an address of second heap byte
   That place always sotres 0 for there would be no prev free block
   the next word stores an address of the successive free block
   If there is no successive one, the content is 0
*/
static unsigned int *starter = 0;

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void realloc_place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void *realloc_coalesce(void *bp, size_t asize, int *is_next_free);
static void printblock(void *bp);
static void check(int verbose);
static void checkblock(void *bp);
static void checkchain(int verbose);
static void mm_check(int verbose);


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
  void *bp;
  mem_init();

  /* Create the initial empty heap */
  if ((heap_listp = (char *)mem_sbrk(6*WSIZE)) == (char *)-1)
    return -1;
  
  starter = (unsigned int *)mem_heap_lo() + 1;
  PUT(heap_listp, 0);	                       /* Alignment padding */
  PUT_PREV(starter, 0);
  PUT_SUCC(starter, 0);			       /* First, starter is alone */
  PUT(heap_listp + (3*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (4*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (5*WSIZE), PACK(0, 1));     /* Epilogue header */
  heap_listp += (4*WSIZE);
  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  bp = extend_heap(CHUNKSIZE/WSIZE);
  if (bp == NULL)
    return -1;
  return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 * Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
  size_t asize;      /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;
  
  if (heap_listp == 0) mm_init();
  
  /* Ignore spurious requests */
  if (size == 0) return NULL;

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE) asize = 2*DSIZE;
  else asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

  /* Search the free list for a fit */
  if ((bp = (char *)find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize,CHUNKSIZE);
  if ((bp = (char *)extend_heap(extendsize/WSIZE)) == NULL) return NULL;
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
  if (bp == 0) return;
  
  size_t size = GET_SIZE(HDRP(bp));
  if (heap_listp == 0) mm_init();
  
  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * chain2starter - chain a free blk to starter
 */
void chain2starter(void* bp)
{
  unsigned int starter_succ_free = GET_SUCC(starter);
  PUT_PREV(bp, starter);
  PUT_SUCC(starter, bp);
  PUT_SUCC(bp, starter_succ_free);
  if(starter_succ_free) PUT_PREV(starter_succ_free, bp);
}

/*
 * chain2prevnext - chain the free prev and next of a free blk
 */
void chain2prevnext(void *bp)
{
  unsigned int succ_free, prev_free;
  succ_free = GET_SUCC(bp);
  prev_free = GET_PREV(bp);
  PUT_SUCC(prev_free, succ_free);
  if(succ_free) PUT_PREV(succ_free, prev_free);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
{
  void *prev_bp = PREV_BLKP(bp), *next_bp = NEXT_BLKP(bp);
  size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));
  size_t next_alloc = GET_ALLOC(HDRP(next_bp));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {            /* Case 1 */
    chain2starter(bp);
  }

  else if (prev_alloc && !next_alloc) {      /* Case 2 */
    size += GET_SIZE(HDRP(next_bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(next_bp), PACK(size,0));
    chain2prevnext(next_bp);
    chain2starter(bp);
  }

  else if (!prev_alloc && next_alloc) {      /* Case 3 */
    size += GET_SIZE(HDRP(prev_bp));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(prev_bp), PACK(size, 0));
    chain2prevnext(prev_bp);
    bp = prev_bp;
    chain2starter(bp);
  }

  else {                                     /* Case 4 */
    size += GET_SIZE(HDRP(prev_bp)) +
      GET_SIZE(FTRP(next_bp));
    PUT(HDRP(prev_bp), PACK(size, 0));
    PUT(FTRP(next_bp), PACK(size, 0));
    chain2prevnext(next_bp);
    chain2prevnext(prev_bp);
    bp = prev_bp;
    chain2starter(bp);
  }
  return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  int is_next_free;
  size_t oldsize, asize, cp_size;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if(size == 0) {
    mm_free(ptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if(ptr == NULL) return mm_malloc(size);
  
  asize = ALIGN(size+DSIZE);
  oldsize = GET_SIZE(HDRP(ptr));
  cp_size = oldsize - DSIZE;

  if(oldsize == asize) return ptr;
  else if(oldsize < asize)
    {
      newptr = realloc_coalesce(ptr, asize, &is_next_free);
      if(is_next_free)
	{
	  realloc_place(newptr, asize);
	  return newptr;
	}
      else if(!is_next_free && newptr != ptr)
	{
	  memcpy(newptr, ptr, cp_size);
	  realloc_place(newptr, asize);
	  return newptr;
	}
      else
	{
	  newptr = mm_malloc(asize);
	  memcpy(newptr, ptr, cp_size);
	  mm_free(ptr);
	  return newptr;
	}
    }
  else
    {
      realloc_place(ptr, asize);
      return ptr;
    }
}

/*
 * mm_check - Check the heap for correctness
 */
void mm_check(int verbose)
{
  check(verbose);
}

/*
 * The remaining routines are internal helper routines
 */

/*
 * extend_heap - Extend heap with free block and return its block pointer
 */
static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if ((long)(bp = (char *)mem_sbrk(size)) == -1) return NULL;

  /* Initialize free block header/footer and the epilogue header */
  PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
  PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
  
  /* Coalesce if the previous block was free */
  return coalesce(bp);
}

/*
 * place - Place block of asize bytes at start of free block bp
 *         and split if remainder would be at least minimum block size
 */
static void place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));

  chain2prevnext(bp);

  if ((csize - asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
    coalesce(bp);
  }
  else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

/*
 * realloc_place - Place block of asize bytes at start of free block bp
 * and split if remainder would be at least minimum block size
 * mostly alike the previous function place, but remove some statements
 * fot its usage in function mm_realloc
 */
static void realloc_place(void *bp, size_t asize)
{
  size_t csize = GET_SIZE(HDRP(bp));

  if ((csize - asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
    coalesce(bp);
  }
  else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
  }
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 * mostly alike the previous function coalesce, but remove some statements
 * fot its usage in function mm_realloc
 */
static void *realloc_coalesce(void *bp, size_t asize, int *is_next_free)
{
  void *prev_bp = PREV_BLKP(bp), *next_bp = NEXT_BLKP(bp);
  size_t prev_alloc = GET_ALLOC(FTRP(prev_bp));
  size_t next_alloc = GET_ALLOC(HDRP(next_bp));
  size_t size = GET_SIZE(HDRP(bp));
  *is_next_free = 0;

  if (prev_alloc && next_alloc) {}           /* Case 1 */

  else if (prev_alloc && !next_alloc) {      /* Case 2 */
    size += GET_SIZE(HDRP(next_bp));
    if(size >= asize)
      {
	PUT(HDRP(bp), PACK(size, 0));
	PUT(FTRP(next_bp), PACK(size,0));
	chain2prevnext(next_bp);
	*is_next_free = 1;
      }
  }

  else if (!prev_alloc && next_alloc) {      /* Case 3 */
    size += GET_SIZE(HDRP(prev_bp));
    if(size >= asize)
      {
	PUT(FTRP(bp), PACK(size, 0));
	PUT(HDRP(prev_bp), PACK(size, 0));
	chain2prevnext(prev_bp);
	bp = prev_bp;
      }
  }

  else {                                     /* Case 4 */
    size += GET_SIZE(HDRP(prev_bp)) +
      GET_SIZE(FTRP(next_bp));
    if(size >= asize)
      {
	PUT(HDRP(prev_bp), PACK(size, 0));
	PUT(FTRP(next_bp), PACK(size, 0));
	chain2prevnext(next_bp);
	chain2prevnext(prev_bp);
	bp = prev_bp;
      }
  }
  return bp;
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
  /* First-fit search */
  unsigned int *bp;
  
  for (bp = (unsigned int *)GET_SUCC(starter); bp != 0; bp = (unsigned int *)GET_SUCC(bp))
    {
      if (asize <= GET_SIZE(HDRP(bp)))
	return (void *)bp;
    }
  return NULL; /* No fit */
}

/*
 * printblock - Print the detail info of a block
 */
static void printblock(void *bp)
{
  size_t hsize, halloc, fsize, falloc;
  
  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%u:%c] footer: [%u:%c]\n", bp,
	 hsize, (halloc ? 'a' : 'f'),
	 fsize, (falloc ? 'a' : 'f'));
}

/*
 * checkblock - Check if a block is not doubleword algined
 * or header-footer not matched
 */
static void checkblock(void *bp)
{
  if ((size_t)bp % 8)
    printf("Error: %p is not doubleword aligned\n", bp);
  if (GET(HDRP(bp)) != GET(FTRP(bp)))
    {
      printf("Error: %p header does not match footer\n", bp);
      printblock(bp);
    }
}

/*
 * checkchain - Check if the double linked list is linked in order
 */
static void checkchain(int verbose)
{
  unsigned int *prev_chain = starter;
  unsigned int *succ_chain = (unsigned int *)GET_SUCC(starter);
  while(succ_chain)
    {
      if(verbose)
	{
	  printf("{%p} -> {%p}\n", prev_chain, (void *)succ_chain);
	  printf("{%p} <- {%p}\n", (void *)GET_PREV(succ_chain), (void *)succ_chain);
	}
      if(prev_chain != (void *)GET_PREV(succ_chain))
	printf("%p, %p unmatch\n", prev_chain, (void *)GET_PREV(succ_chain));
      prev_chain = (unsigned int *)succ_chain;
      succ_chain = (unsigned int *)GET_SUCC(succ_chain);
    }
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void check(int verbose)
{
  char *bp = heap_listp;

  checkchain(verbose);
  if (verbose)
    printf("Heap (%p):\n", heap_listp);

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
    printf("Bad prologue header\n");
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose)
      printblock(bp);
    checkblock(bp);
  }
  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
    printf("Bad epilogue header\n");
  if(verbose)
    printblock(bp);
}
