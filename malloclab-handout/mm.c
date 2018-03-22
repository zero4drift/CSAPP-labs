/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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

/* implict free list */
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

/* Global variables */
static char *heap_listp = 0;  /* Pointer to first block */
/* 
   A pointer whose value is heap_listp + 4
   That place stores a pointer of the successive free block
   If there is no successive one, the content is 0
*/
static unsigned int *starter = NULL;

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void check(int verbose);
static void checkblock(void *bp);
static void checkchain(unsigned int *starter);
void mm_check(int verbose);
/* end implict free list */


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
/* first based on implict free list */
{
  void *bp;
  mem_init();

  /* Create the initial empty heap */
  if ((heap_listp = (char *)mem_sbrk(4*WSIZE)) == (char *)-1)
    return -1;
  PUT(heap_listp, 0);	                      /* Alignment padding */
  /*
    starter is always the inital heap_listp + 4 
    It stores the address of successive free block
  */
  starter = (unsigned int *)(heap_listp);
  *starter = 0;
  PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* Prologue header */
  PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
  PUT(heap_listp + (3*WSIZE), PACK(0, 1));     /* Epilogue header */
  heap_listp += (2*WSIZE);
  /* Extend the empty heap with a free block of CHUNKSIZE bytes */
  bp = extend_heap(CHUNKSIZE/WSIZE);
  if (bp == NULL)
    return -1;
  return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
/* first based on implict free list */
{
  size_t asize;      /* Adjusted block size */
  size_t extendsize; /* Amount to extend heap if no fit */
  char *bp;
  
  if (heap_listp == 0){
    mm_init();
  }
  
  /* Ignore spurious requests */
  if (size == 0)
    return NULL;

  /* Adjust block size to include overhead and alignment reqs. */
  if (size <= DSIZE)
    asize = 2*DSIZE;
  else
    asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

  /* Search the free list for a fit */
  if ((bp = (char *)find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  /* No fit found. Get more memory and place the block */
  extendsize = MAX(asize,CHUNKSIZE);
  /* printf("NO FIT\n"); */
  if ((bp = (char *)extend_heap(extendsize/WSIZE)) == NULL)
    return NULL;
  /* printf("Is that bp ok %p\n", bp); */
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
/* first based on implict free list */
{
  if (bp == 0)
    return;
  
  size_t size = GET_SIZE(HDRP(bp));
  if (heap_listp == 0){
    mm_init();
  }

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));
  coalesce(bp);
}

/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
static void *coalesce(void *bp)
/* first based on a implict free list */
{
  unsigned int succ_free, prev_free;
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {            /* Case 1 */
    /*
      Starter always stores the address of next free block
      First double word of free block stores the address of previous block
      or the address of starter
      There is no successive free block, so the next double word stores 0
    */
    succ_free = *starter;
    /* printf("starter succ %p\n", (void *)succ_free); */
    prev_free = (unsigned int)starter;
    if(!succ_free)
      {
	*starter = (unsigned int)bp;
	*((unsigned int *)bp) = (unsigned int)starter;
	*((unsigned int *)bp + 1) = 0;
	/* printf("%p\n", (void *)(*(unsigned int *)bp)); */
      }
    else
      {
	/* printf("%p\n", (void *)bp); */
	for(succ_free = *starter; succ_free != 0; succ_free = *((unsigned int *)succ_free + 1))
	  {
	    if(succ_free > (unsigned int)bp)
	      {
		/* printf("prev_free %p\n", (void *)prev_free); */
		/* printf("second starter succ %p\n", (void *)succ_free); */
		*((unsigned int *)succ_free) = (unsigned int)bp;
		*((unsigned int *)bp + 1) = (unsigned int)succ_free;
		if(prev_free == (unsigned int)starter)
		  {
		    *((unsigned int *)prev_free) = (unsigned int)bp;
		  }
		else
		  {
		    *((unsigned int *)prev_free + 1) = (unsigned int)bp;
		  }
		*((unsigned int *)bp) = (unsigned int)prev_free;
		return bp;
	      }
	    prev_free = succ_free;
	  }
	*((unsigned int *)bp + 1) = 0;
	*((unsigned int *)prev_free + 1) = (unsigned int)bp;
	*((unsigned int *)bp) = (unsigned int)prev_free;
      }
    return bp;
  }

  else if (prev_alloc && !next_alloc) {      /* Case 2 */
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    /* printf("SIZE!!!!! %d\n", (int)size); */
    prev_free = *((unsigned int *)(NEXT_BLKP(bp)));
    succ_free = *((unsigned int *)(NEXT_BLKP(bp)) + 1);
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size,0));
    *((unsigned int *)bp) = prev_free;
    *((unsigned int *)bp + 1) = succ_free;
    if(succ_free)
      {
	*((unsigned int *)succ_free) = (unsigned int)bp;
      }
    if(prev_free == (unsigned int)starter)
      {
	*((unsigned int *)prev_free) = (unsigned int)bp;
      }
    else
      {
	*((unsigned int *)prev_free + 1) = (unsigned int)bp;
      }
  }

  else if (!prev_alloc && next_alloc) {      /* Case 3 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp)));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }

  else {                                     /* Case 4 */
    size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
      GET_SIZE(FTRP(NEXT_BLKP(bp)));
    succ_free = *((unsigned int *)NEXT_BLKP(bp) + 1);
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
    *((unsigned int *)bp + 1) = succ_free;
    if(succ_free)
      {
	*((unsigned int *)succ_free) = (unsigned int)bp;
      }
  }
  return bp;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
  size_t oldsize;
  void *newptr;

  /* If size == 0 then this is just free, and we return NULL. */
  if(size == 0) {
    mm_free(ptr);
    return 0;
  }

  /* If oldptr is NULL, then this is just malloc. */
  if(ptr == NULL) {
    return mm_malloc(size);
  }

  newptr = mm_malloc(size);

  /* If realloc() fails the original block is left untouched  */
  if(!newptr) {
    return 0;
  }

  /* Copy the old data. */
  oldsize = GET_SIZE(HDRP(ptr));
  if(size < oldsize) oldsize = size;
  memcpy(newptr, ptr, oldsize);

  /* Free the old block. */
  mm_free(ptr);

  return newptr;
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
/* $begin mmextendheap */
static void *extend_heap(size_t words)
{
  char *bp;
  size_t size;

  /* Allocate an even number of words to maintain alignment */
  size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
  if ((long)(bp = (char *)mem_sbrk(size)) == -1)
    return NULL;
  /* printf("extend %p\n", bp); */

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
  unsigned int prev_free, succ_free;
  prev_free = *((unsigned int *)bp);
  succ_free = *((unsigned int *)(bp) + 1);

  if ((csize - asize) >= (2*DSIZE)) {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    bp = NEXT_BLKP(bp);
    /* printf("left space address %p\n", (void *)bp); */
    /* printf("prev_free %p\n", (void *)prev_free); */
    /* printf("succ_free %p\n", (void *)succ_free); */
    /* printf("starter succ %p\n", (void *)(*starter)); */
    PUT(HDRP(bp), PACK(csize-asize, 0));
    PUT(FTRP(bp), PACK(csize-asize, 0));
    /* mm_check(0); */
    /* checkchain(starter); */
    /* printf("\n"); */
    if(succ_free)
      {
	*((unsigned int *)succ_free) = (unsigned long)bp;
      }
    if(prev_free == (unsigned int)starter)
      {
	*starter = (unsigned int)bp;
	/* printf("starter succ2 %p\n", (void *)(*starter)); */
      }
    else
      {
	*((unsigned int *)prev_free + 1) = (unsigned int)bp;
      }
    *((unsigned int *)bp) = prev_free;
    *((unsigned int *)bp + 1) = succ_free;
  }
  else {
    PUT(HDRP(bp), PACK(csize, 1));
    PUT(FTRP(bp), PACK(csize, 1));
    /* printf("prev_free %p, succ_free %p\n", (void *)prev_free, (void *)succ_free) */;
    if(prev_free == (unsigned int)starter && succ_free)
      {
	*starter = succ_free;
	*((unsigned int *)succ_free) = prev_free;
      }
    else if(prev_free == (unsigned int)starter && !succ_free)
      {
	*starter = succ_free;
      }
    else if(!succ_free)
      {
	*((unsigned int *)prev_free + 1) = succ_free;
      }
    else
      {
	*((unsigned int *)prev_free + 1) = succ_free;
	*((unsigned int *)succ_free) = prev_free;
      }
  }
}

/*
 * find_fit - Find a fit for a block with asize bytes
 */
static void *find_fit(size_t asize)
{
  /* First-fit search */
  unsigned int bp;

  /* printf("starter succ3 %p\n", (void *)(*starter)); */
  for (bp = *starter; bp != 0; bp = *((unsigned int *)bp + 1)) {
    /* printf("looping %p size %d asize %d\n", (void *)bp, GET_SIZE(HDRP(bp)), (int)asize); */
    if (asize <= GET_SIZE(HDRP(bp))) {
      /* printf("fit address %p\n", (void *)bp); */
      return (void *)bp;
    }
  }
  return NULL; /* No fit */
}

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

static void checkchain(unsigned int *starter)
{
  unsigned int *prev_chain = starter;
  unsigned int succ_chain = *starter;
  while(succ_chain)
    {
      printf("{%p} -> {%p}\n", prev_chain, (void *)succ_chain);
      printf("{%p} <- {%p}\n", (void *)(*((unsigned int *)succ_chain)), (void *)succ_chain);
      prev_chain = (unsigned int *)succ_chain;
      succ_chain = *((unsigned int *)(succ_chain) + 1);
    }
  printf("Stopped check chain\n");
}

/*
 * checkheap - Minimal check of the heap for consistency
 */
void check(int verbose)
{
  char *bp = heap_listp;

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
