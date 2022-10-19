/*
 * mm-csapp.c - Following codes at CSAPP.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"



#define WSIZE       4
#define DSIZE       8
#define CHUNKSIZE   (1<<8)

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)      (*(unsigned int *)(p))
#define PUT(p, val)   (*(unsigned int *)(p) = val)

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)    ((char *)bp - WSIZE)
#define FTRP(bp)    ((char *)bp + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)   ((char *)bp + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)   ((char *)bp - GET_SIZE((char *)HDRP(bp) - WSIZE))

#define FIRST_FIT   0
#define NEXT_FIT    1
#define BEST_FIT    2

#define ALIGN(size) (((size) + (DSIZE - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

void* heap_listp;
void* last_found;

/*
 * Util Functions
 */
static void* coalesce(void* bp) {
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)
        return bp;
    
    else if (!prev_alloc && next_alloc) {
        if (bp == last_found) last_found = PREV_BLKP(bp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else if (prev_alloc && !next_alloc) {
        if (NEXT_BLKP(bp) == last_found) last_found = bp;

        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    else {
        if (bp == last_found || NEXT_BLKP(bp) == last_found) last_found = PREV_BLKP(bp);

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    return bp;
}

static void* extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words + 1) / 2 * DSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;
    
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    return coalesce(bp);
}

static void* find_fit(size_t asize) {
    int findType = BEST_FIT;
    size_t extendsize;
    void* lastBlockPointer;

    // FIRST FIT
    if (findType == FIRST_FIT) {
        void* finderBp = heap_listp;
        while (1) {
            // if found fit in free list
            if (
                GET_ALLOC(HDRP(finderBp)) == 0 
                && GET_SIZE(HDRP(finderBp)) >= asize
            ) break;

            // if reached epilogue of free list
            if (GET(HDRP(finderBp)) == 0x1) {
                return NULL;
            }

            // check next
            finderBp = NEXT_BLKP(finderBp);
        }
        return finderBp;
    }

    // NEXT FIT
    else if (findType == NEXT_FIT) {
        void* finderBp = NEXT_BLKP(last_found);
        while (1) {
            // if found fit in free list
            if (
                GET_ALLOC(HDRP(finderBp)) == 0 
                && GET_SIZE(HDRP(finderBp)) >= asize
            ) {
                last_found = finderBp;
                return finderBp;
            }

            // if searched every free list
            if (finderBp == last_found) {
                return NULL;
            }

            // check next
            if(GET(HDRP(finderBp)) == 0x1) 
                finderBp = heap_listp;
            else
                finderBp = NEXT_BLKP(finderBp);

        }
    }

    // BEST FIT
    else if (findType == BEST_FIT) {
        void* finderBp = heap_listp;
        void* bestBp = NULL;
        size_t bestSize = -1;   // max size

        while (1) {
            size_t nowSize = GET_SIZE(HDRP(finderBp));

            // if perfect fit
            if ((GET_ALLOC(HDRP(finderBp)) == 0) && nowSize == asize) {
                bestBp = finderBp;
                bestSize = nowSize;
                break;
            }

            // if good fit
            if ((GET_ALLOC(HDRP(finderBp)) == 0) && asize < nowSize && nowSize < bestSize) {
                bestBp = finderBp;
                bestSize = nowSize;
            }

            // if reached epilogue
            size_t headerContent = GET(HDRP(finderBp));
            if (headerContent == 0x1) break;

            finderBp = NEXT_BLKP(finderBp);
        }

        // no fit case
        if (bestSize == -1) {
            return NULL;
        }
        
        // best fit case
        return bestBp;
    }

    return NULL;
}

static void place(void* bp, size_t asize) {
    size_t fsize = GET_SIZE(HDRP(bp));

    // if right fit
    if (fsize == asize) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
    }
    // if to be split
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(fsize - asize, 0));
        PUT(FTRP(bp), PACK(fsize - asize, 0));
    }
}



/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp + (0*WSIZE), 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));
    heap_listp += (2*WSIZE);

    last_found = heap_listp;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
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
        place(bp, asize);
        return bp;
    }

    // no fit case
    size_t lastBp = heap_listp;
    while (GET(HDRP(NEXT_BLKP(lastBp))) != 0x1) lastBp = NEXT_BLKP(lastBp);
    if (GET_ALLOC(HDRP(lastBp)) == 0) {
        extendsize = asize - GET_SIZE(HDRP(lastBp));
    }
    else {
        extendsize = asize;
    }

    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) 
        return NULL;
    place(bp, asize);
    last_found = bp;

    return bp;
}

/*
 * mm_free
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    void* fb = coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














