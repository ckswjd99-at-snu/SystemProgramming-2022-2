/*
 * mm-seg.c - implementing segregation list.
 * 
 * Total seglist is SEGSIZE.
 * Starting point of every seglist is saved at starting area of heap memory.
 * If statring point of a seglist is NULL, it means it's empty.
 * Each seglist is ascending doubly linked list.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"


/*
 * Macros - common
 */

#define WSIZE       4
#define DSIZE       8
#define INITCHUNKSIZE   (1<<6)
#define CHUNKSIZE   (1<<12)
#define SPLIT_THRES 100

#define MAX(x, y)   ((x) > (y) ? (x) : (y))
#define MIN(x, y)   ((x) < (y) ? (x) : (y))

#define ALIGNMENT   DSIZE
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val)   (*(unsigned int *)(p) = val)

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define GET_LBIT(p, n)      ((GET(p) >> n) & 0x1)
#define PUT_LBIT(p, n, v)   (*(unsigned int *)(p) = (*(unsigned int *)p & ~(1 << n)) | (v << n))

#define HDRP(bp)    ((char *)bp - WSIZE)
#define FTRP(bp)    ((char *)bp + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)   ((char *)bp + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)   ((char *)bp - GET_SIZE((char *)HDRP(bp) - WSIZE))


/*
 * Macros - segregation
 */

#define SEGSIZE     50

#define PREDP(bp)   ((unsigned int *)bp)
#define SUCCP(bp)   ((unsigned int *)((char *)bp + WSIZE))

#define PRED_BLKP(bp)   (*((unsigned int *)bp))
#define SUCC_BLKP(bp)   (*((unsigned int *)((char *)bp + WSIZE)))


/*
 * static scalar variables
 */

static void* heap_listp;
static void** seg_listp;


/*
 * Util Functions - maths
 */

size_t CEIL_POW2_IDX (size_t val) {
    size_t idx = 0;

    if (val <= 1)
        return 0;
    
    val--;
    val >>= 1;
    while(val != 0) {
        val >>= 1;
        idx++;
    }
    
    return idx+1;
}


/*
 * Util Functions - seglist
 */

static void** find_seglist (size_t asize) {
    return seg_listp + CEIL_POW2_IDX(asize);
}

static void* find_fit_in_segindex (size_t segindex, size_t asize) {
    void* finder_bp = *(unsigned int *)((char *)seg_listp + segindex * WSIZE);

    if (finder_bp == NULL) return NULL;

    while (1) {
        if (GET_SIZE(HDRP(finder_bp)) >= asize)
            return finder_bp;
        finder_bp = SUCC_BLKP(finder_bp);
        if(finder_bp == NULL) break;
    }

    return NULL;
}

static void* find_fit(size_t asize) {
    size_t seglist_idx = MIN(CEIL_POW2_IDX(asize), SEGSIZE - 1);
    void* finder_bp;

    while (1) {
        finder_bp = find_fit_in_segindex(seglist_idx, asize);

        if (finder_bp != NULL || seglist_idx >= SEGSIZE - 1) break;
        seglist_idx++;
    }
    
    return finder_bp;
}

static void pop_from_seglist (void* bp) {
    void** seglist = find_seglist(GET_SIZE(HDRP(bp)));
    
    // if bp is only elem of the seglist
    if (PRED_BLKP(bp) == NULL && SUCC_BLKP(bp) == NULL) {
        *seglist = NULL;
    }

    // if bp is head of the seglist
    else if (PRED_BLKP(bp) == NULL) {
        *seglist = SUCC_BLKP(bp);
        PUT(PREDP(SUCC_BLKP(bp)), NULL);
    }

    // if bp is last of the seglist
    else if (SUCC_BLKP(bp) == NULL) {
        PUT(SUCCP(PRED_BLKP(bp)), NULL);
    }

    // if bp is in the middle of the seglist
    else {
        PUT(SUCCP(PRED_BLKP(bp)), SUCC_BLKP(bp));
        PUT(PREDP(SUCC_BLKP(bp)), PRED_BLKP(bp));
    }
}

static void push_in_seglist (void* bp) {
    void** seglist = find_seglist(GET_SIZE(HDRP(bp)));
    size_t asize = GET_SIZE(HDRP(bp));

    // if seglist is empty
    if (*seglist == NULL) {
        *seglist = bp;
        PUT(PREDP(bp), NULL);
        PUT(SUCCP(bp), NULL);
        return;
    }

    // push in seglist, maintaining aescending order
    void* finder_bp = *seglist;
    if (asize < GET_SIZE(HDRP(finder_bp))) {
        *seglist = bp;
        PUT(PREDP(bp), NULL);
        PUT(SUCCP(bp), finder_bp);
        PUT(PREDP(finder_bp), bp);
        return;
    }

    while (SUCC_BLKP(finder_bp) != NULL && GET_SIZE(HDRP(SUCC_BLKP(finder_bp))) < asize) {
        finder_bp = SUCC_BLKP(finder_bp);
    }

    // if finder_bp is tail of the seglist
    if (SUCC_BLKP(finder_bp) == NULL) {
        PUT(SUCCP(finder_bp), bp);
        PUT(PREDP(bp), finder_bp);
        PUT(SUCCP(bp), NULL);
    }
    // if finder_bp is not tail
    else {
        PUT(PREDP(bp), finder_bp);
        PUT(SUCCP(bp), SUCC_BLKP(finder_bp));
        PUT(SUCCP(finder_bp), bp);
        PUT(PREDP(SUCC_BLKP(bp)), bp);
    }
}



static void* coalesce(void* bp) {
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (GET_LBIT(HDRP(PREV_BLKP(bp)), 1) == 1) 
        prev_alloc = 1;
    

    if (prev_alloc && next_alloc)
        return bp;
    
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));

        pop_from_seglist(bp);
        pop_from_seglist(PREV_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        push_in_seglist(bp);
        
    }

    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));

        pop_from_seglist(bp);
        pop_from_seglist(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        push_in_seglist(bp);
    }

    else {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));

        pop_from_seglist(bp);
        pop_from_seglist(PREV_BLKP(bp));
        pop_from_seglist(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        push_in_seglist(bp);
    }

    return bp;
}

static void* extend_heap(size_t words) {
    char *bp;
    size_t asize;

    asize = (words + 1) / 2 * DSIZE;
    
    if ((bp = mem_sbrk(asize)) == (void *)-1)
        return NULL;
    
    // Set headers and footer 
    PUT(HDRP(bp), PACK(asize, 0));  
    PUT(FTRP(bp), PACK(asize, 0));   
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); 
    push_in_seglist(bp);


    return coalesce(bp);
}

static void* place(void* bp, size_t asize) {
    size_t fsize = GET_SIZE(HDRP(bp));

    // if right fit
    if (fsize < asize + WSIZE * 4) {
        pop_from_seglist(bp);
        PUT(HDRP(bp), PACK(fsize, 1));
        PUT(FTRP(bp), PACK(fsize, 1));
    }
    // if to be splited
    else if (asize < SPLIT_THRES) {
        pop_from_seglist(bp);

        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        PUT(HDRP(NEXT_BLKP(bp)), PACK(fsize - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(fsize - asize, 0));
        
        push_in_seglist(NEXT_BLKP(bp));
    }
    else {
        pop_from_seglist(bp);

        PUT(HDRP(bp), PACK(fsize - asize, 0));
        PUT(FTRP(bp), PACK(fsize - asize, 0));

        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));

        push_in_seglist(bp);
        bp = NEXT_BLKP(bp);
    }

    return bp;
}


// API functions

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    void* brk;
    int i;

    // init seglist
    if ((brk = mem_sbrk(SEGSIZE * WSIZE)) == (void *)-1)
        return -1;
    
    seg_listp = brk;
    
    for (i=0; i<SEGSIZE; i++) 
        *(seg_listp + i) = NULL;

    // init heap
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp + (0*WSIZE), 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    if (extend_heap(INIT_HEAP) == NULL)
        return -1;
    
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char* bp;

    if (size == 0)
        return NULL;
    
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = (size + DSIZE + (DSIZE - 1)) / DSIZE * DSIZE;
    
    if ((bp = find_fit(asize)) != NULL) {
        bp = place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);

    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) 
        return NULL;
    bp = place(bp, asize);

    return bp;
}

/*
 * mm_free
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT_LBIT(HDRP(NEXT_BLKP(ptr)), 1, 0);
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    push_in_seglist(ptr);
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    size_t asize;
    size_t extendsize;
    size_t last_free_size;

    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = (size + DSIZE + (DSIZE - 1)) / DSIZE * DSIZE;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














