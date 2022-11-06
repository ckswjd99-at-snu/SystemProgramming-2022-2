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


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// 선형에서 가장 가까운 배수로 올림?
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)   /* Extend heap by amount (bytes) */

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))    // 크기와 할당 비트를 통합해서 헤더와 풋터에 저장할 수 있는 값 리턴

/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))       // p가 참조하는 헤더 리턴
#define PUT(p, val) (*(unsigned int *)(p) = (val))  // p가 가리키는 워드에 val 저장

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     (GET(p) & ~0x7) // 헤더 사이즈, 할당 비트 리턴
#define GET_ALLOC(p)    (GET(p) & 0x1) // 풋터 사이즈, 할당 비트 리턴

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)    ((char *)(bp) - WSIZE)  // 헤더를 가리키는 포인터 리턴
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) // 풋터를 가리키는 포인터 리턴

/* Given block ptr bp, comupte address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) // 다음 블록 포인터 리턴
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // 이전 블록 포인터 리턴

/* 추가로 선언한 함수들 */
static void *heap_listp;
static void *last_found;       // 이 변수는 다른 파일에서 여러번 접근할 때도 계속 기존 값을 유지해야 하므로 static이 필요하다.
/* last_bp를 업데이트 해주는 경우는 총 3가지. 첫 init때 last_bp = heap_listp로 초기화해주는 과정.
 * 다음은 place로 블록을 할당해줄 때 bp가 바뀌므로 한번.
 * coalesce로 블록 합치는 과정에서 앞 블록과 합쳐질 경우 bp가 바뀌므로 한번.
*/

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *first_fit(size_t asize);
static void *next_fit(size_t asize);
static void place(void *bp, size_t asize);

/* 
 * mm_init - initialize the malloc package.
 */
 // 힙 리스트를 힙에서 필요한 만큼 가져오기. 
int mm_init(void)
{
    /* Create the initial empty heap */
    // mem_sbrk : 필요한 크기의 메모리가 힙 영역에 있는지 확인하고 만약 있다면 그 포인터 주소를, 아니면 -1 출력. 포인터만 옮기는 것
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)     // 힙의 최대 크기보다 더 큰 메모리를 요청할 경우 -1
        return -1;

    PUT(heap_listp, 0);                             /* Alignment padding */
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    /* Prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));        /* Epliogue header */
    heap_listp += (2*WSIZE);        // 포인터를 프롤로그의 footer로 이동

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    // 힙 영역에 메모리 요청
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    last_found = heap_listp;   
    return 0;
}

// 힙을 초기화할 때 또는 추기 힙 공간을 요청할 때
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain aligment */
    // 요청받은 크기를 2워드 배수(8byte)로 반올림. 그리고 힙 공간 요청
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;     // words % 2 == 0이면 false니까 else문 동작
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;
    // 현재 bp 위치 : 에필로그 헤더 뒷부분

    /* Initialize free block header/footer and the epilogue header */
    // 새로운 힙 영역의 여유 블록의 헤더, 풋터, 에필로그 헤더 초기화
    PUT(HDRP(bp), PACK(size, 0));       /* Free block header */     // 에필로그 헤더 뒷부분에서 -4byte면 에필로그헤더 앞부분. 여기를 헤더로 초기화
    PUT(FTRP(bp), PACK(size, 0));       /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));   /* New epliogue header */

    /* Coalesce if the previous block was free */
    // 이전 블록이 할당되지 않았다면 이후 받아온 힙 영역과 합체
    return coalesce(bp);
}

// 맨 앞 블록에서부터 하나씩 여유 공간 서치.
static void *first_fit(size_t asize)
{
    /* First-fit search */
    void *bp;

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
            return bp;

    }
    return NULL;    /* No fit */
}

// 이전 포인터에서부터 뒤로 하나씩 여유 공간 서치
static void *next_fit(size_t asize)
{
    /* Next-fit search */
    void *bp;

    for (bp = last_found; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp))
    {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))
        {
            return bp;
        }
    }
    return NULL;    /* No fit */
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    // int newsize = ALIGN(size + SIZE_T_SIZE);
    // void *p = mem_sbrk(newsize);
    // if (p == (void *)-1)
	// return NULL;
    // else {
    //     *(size_t *)p = size;
    //     return (void *)((char *)p + SIZE_T_SIZE);
    // }

    size_t asize;       /* Adjusted block size */       // 넣을 블록의 크기
    size_t extendsize;  /* Amount to extend heap if no fit */       // 힙을 확장할 크기
    char *bp;

    /* Ignore spurious requests */
    if (size <= 0)
        return NULL;

    /* Adjust block size to include overhead and aligment reqs. */
    /* 블록의 크기 정하기. 헤더 + 데이터 + 풋터 + 패딩 합쳐서 8의 배수가 되도록 설정 */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    /* Search the free list for a fit */
    // if ((bp = first_fit(asize)) != NULL)
    // 가용 공간 서치
    if ((bp = next_fit(asize)) != NULL)
    {
        place(bp, asize);   // bp : 데이터를 할당할 수 있는 공간의 시작 위치
        // last_bp = HDRP(NEXT_BLKP(bp));       // 깔끔한 건 할당한 그 공간의 다음 블록의 헤더를 가리키는 것. 여유 공간 서치 함수의 첫 시작위치가 현재 위치의 풋터이므로 이렇게 짜면 안된다.
        last_found = bp;
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    // 가용 공간 없음. 이 모형에서는 청크사이즈 단위로 힙을 추가로 받아오는데 받아올 사이즈가 더 크면 그 사이즈만큼 받아옴
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    // last_bp = HDRP(NEXT_BLKP(bp));
    last_found = bp;
    return bp;
}

// 데이터를 할당할 가용 블록의 bp와 배치 용량 할당
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));      // 현재 블록의 크기.
    
    // 현재 사이즈가 할당할 사이즈보다 당연히 클텐데. 그 차이가 더블워드*2 이상이면? 헤더풋터가 합쳐서 8바이트니까 데이터까지 집어넣고 8의배수 맞추려면 최소 더블워드*2만큼 있어야지.
    // 필요한 만큼 할당하고 그 이후 공간은 빈 공간으로 남기기
    if ((csize - asize) >= (2*DSIZE))       
    {
        PUT(HDRP(bp), PACK(asize, 1));          // 헤더에 할당할 사이즈 업데이트
        PUT(FTRP(bp), PACK(asize, 1));          // 풋터에 할당할 사이즈 업데이트
        bp = NEXT_BLKP(bp);                     // bp를 다음 블록의 앞부분으로 옮기기
        PUT(HDRP(bp), PACK(csize-asize, 0));    // 할당하고 남은 크기만큼 가용 공간 남기기, 헤더에 사이즈 업데이트
        PUT(FTRP(bp), PACK(csize-asize, 0));    // 풋터에 사이즈 업데이트
    }

    // 아니면 현재 블록 크기만큼 할당
    else 
    {
        PUT(HDRP(bp), PACK(csize, 1));      // 현재 블록에 사이즈 업데이트
        PUT(FTRP(bp), PACK(csize, 1));
    }

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 내 앞 블록의 풋터 블록의 크기
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 내 뒷 블록의 헤더 블록의 크기
    size_t size = GET_SIZE(HDRP(bp));   // 현재 접근 중인 블록의 크기

    if (prev_alloc && next_alloc)   // Case 1       // 둘다 0이 아닌 경우
    {
        return bp;
    }

    else if (prev_alloc && !next_alloc) // Case 2   // 뒤가 0인 경우
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));      // 뒤 블록의 크기 더하기
        PUT(HDRP(bp), PACK(size, 0));               // 헤더에 사이즈 업데이트
        PUT(FTRP(bp), PACK(size, 0));               // 풋터에 사이즈 업데이트
    }

    else if (!prev_alloc && next_alloc)     // Case 3   // 앞 블록이 0인 경우
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));      // 앞 블록의 사이즈 더하기
        PUT(FTRP(bp), PACK(size, 0));               // 현재 블록의 풋터 사이즈 업데이트
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 앞 블록의 헤더 사이즈 업데이트
        bp = PREV_BLKP(bp);                         // bp를 앞 블록 헤더로 옮기기
    }

    else        // Case 4       // 앞뒤 모두 사이즈가 0인 경우
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  // 앞뒤 블록의 사이즈 더하기
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));    // 앞 블록 헤더 사이즈 업데이트
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));    // 뒤 블록 풋터 사이즈 업데이트
        bp = PREV_BLKP(bp);                         // 앞 블록으로 헤더 옮기기
    }
    last_found = bp;       // bp의 변화가 있으므로 이 시점에 전역변수 업데이트
    return bp;
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
    // copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
        copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}