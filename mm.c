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
    "team4",
    /* First member's full name */
    "yudaegyeom",
    /* First member's email address */
    "momo9341@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
/* header and footer size (4+4=8byte) */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)
#define MAX(x,y) ((x>y)?(x):(y))
/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size)|(alloc))                                  // 워드에는 size와 alloc 값
/* Read and write a word at address p */
#define GET(p)      (*(unsigned int *)(p))                                  // void 포인터를 unsigned int를 가리키는 포인터로 변환 후, 그 포인터가 가리키는 정수값  
#define PUT(p,val)  (*(unsigned int *)(p)=(val))                            // void 포인터를 unsigned int를 가리키는 포인터로 변환 후, 그 포인터가 가리키는 정수값을 바꿈
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)     ((GET(p)) & ~0x7)                                   // ~0x7(=11111000)과 헤더 or 푸터와 연산시 블록사이즈 구함
#define GET_ALLOC(p)    ((GET(p)) & 0x1)                                    //  0x1(=00000001)과 헤더 or 푸터와 연산시 할당여부(1,0) 구함
/* Given block ptr bp, compute address of ints header and footer */
#define HDRP(bp)        ((char *)(bp) - WSIZE)                              // bp에서 4byte 이전이 헤더
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)           // bp에서 블록사이즈만큼 이후로 가서 8byte 이전이 푸터
/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp)-WSIZE)))     // bp에서 bp의 헤더에서 구한 bp 사이즈를 더하면 NEXT bp
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp)-DSIZE)))     // bp에서 PREV bp의 푸터에서 구한 PREV bp 사이즈를 빼면 PREV bp

/* 전역 변수  */
static char *heap_listp;        // prologue 블록 포인터

/* first-fit의 문제점: 처음부터 순차적으로 탐색해서 할당할 블록을 찾기 때문에 큰 가용 블록은 주로 implicit list의 뒤쪽에 분포하게 된다. 
-> 큰 사이즈를 할당할 경우 시간이 오래 걸림 , 문제개선을 위해 NEXT_FIT으로 검색 블록 포인터의 블록부터 검색하도록 한다. */

/* 검색을 시작할 블록의 포인터 -> 처음부터 순차적으로 검색하지 않고 검색 블록 포인터부터 검색하도록 한다. 
검색 블록 포인터는 init(초기화)시에 heap_listp로 초기화하고, coalesce(연결) 시에 연결된 블록의 포인터로 갱신된다 */
#define NEXT_FIT  
/* idea: coalesce 함수를 거친 블록은 거치기 전 블록보다 크거나 같다. -> coalesce를 거치면 가용 블록의 크기가 커질 확률이 높다.
-> 해당 블록을 검색의 시작으로 하면 큰 사이즈를 할당할 때 first-fit보다 검색시간이 줄어들 확률이 높다. */ 
#ifdef NEXT_FIT                 
    static void *search_p;      
#endif 
/* first-fit, next-fit의 문제점: 블록의 크기를 덜 신경 쓰기 때문에 메모리 효율이 떨어진다. */
/* idea: 모든 가용블록중에서 크기가 맞는 가장 작은 블록 선택 */
// #define BEST_FIT

/* 정리 */
// 속도: next > first > best
// 메모리효율: best > first >~ next 

/* 원형 선언 */
static void *extend_heap(size_t);
static void *coalesce(void *);
static void *find_fit(size_t);
static void place(void *, size_t);

int mm_init(void);
void *mm_malloc(size_t);
void mm_free(void *);
void *mm_realloc(void *, size_t);


/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;                                 // 크기를 조절하고 싶은 블록의 시작 포인터 oldptr
    void *newptr;                                       // 크기 조절 뒤의 새 블록의 시작 포인터 newptr
    size_t copySize;                                    // 복사할 블록의 데이터(payload+padding) 크기
    
    newptr = mm_malloc(size);                           // 할당할 size에 해당하는 블록을 생성하고 그 주소를 newptr에 반환
    if (newptr == NULL) return NULL;                    // size=0이거나 workspace가 heap을 넘어가면 할당하지 못하고 NULL 반환     
    copySize = GET_SIZE(HDRP(oldptr)) - SIZE_T_SIZE;    // oldptr이 가리키는 데이터(payload+padding)의 사이즈를 copySize에 저장
    if (size < copySize)    copySize = size;            // size가 copySize보다 작다면 size만큼만 oldptr에서 newptr에서 메모리를 복사할 수 있다.                       
    memcpy(newptr, oldptr, copySize);                   // newptr에 oldptr을 시작으로 copySize만큼의 메모리값을 복사한다
    mm_free(oldptr);                                    // oldptr의 메모리 반환
    return newptr;                                      // 재할당한 블록의 주소 리턴
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)   return -1;    // 16byte만큼의 work space를 만들고, 시작 주소를 heap_listp에 반환
    PUT(heap_listp,0);                                                  // Unused 
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));                      // Prologue header (8/1)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));                      // Prologue footer (8/1)
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));                          // Epilogue header (0/1)
    heap_listp += (2 * WSIZE);                                          // heap_listp가 Prologue의 블록포인터가 됨 (이후로 쭉 고정)
    /* next_fit 사용할 경우 검색 시작 포인터를 Prologue의 블록포인터로 한다.*/
    #ifdef NEXT_FIT                                                     
        search_p = heap_listp;                                           
    #endif
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)             return -1;    // (CHUNKSIZE/0) header가 Epiligue header를 덮어쓰고, work space는 CHUNKSIZE byte만큼 확장, 끝에는 epilogue header
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;                           // size를 담을 수 있는 최소 블록 사이즈 
    size_t extendsize;                      // 가용 블록에 asize보다 큰 블록이 없는 경우 -> work space를 확장할 때 extendsize 사용
    char *bp;
    if (size==0)    return NULL;                                        // 비정상적인 입력
    asize = ALIGN(size + SIZE_T_SIZE);      // 블록은 헤더와 푸터(8byte)를 포함하고, payload와 padding의 합이 8의 배수여야 한다. // 만약 size가 2라면, 담을수 있는 최소 블록 사이즈(asize)은 16byte가 된다.      
    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL)     // asize보다 크거나 같은 가용 블록(size>=asize, alloc=0)을 찾아서 있으면 해당 블록의 포인터를 반환
    {
        place(bp,asize);                    // 반환된 블록 포인터로 접근해서 헤더와 푸터 변경(place 함수 참조)
        return bp;                          // 할당된 메모리의 주소 반환 
    }
    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);     // asize를 담을 블록을 찾을 수 없을 경우 work space의 크기를 늘린다. asize가 CHUNKSIZE보다 크면 asize만큼 work space를 늘린다.
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)   return NULL;    // 허용한 heap의 범위를 넘어가는 경우에는 workspace를 확장하지 않고 NULL 반환
    place(bp,asize);                        // workspace 확장 후 반환된 블록 포인터로 접근해서 헤더와 푸터 변경(place 함수 참조)
    return bp;                              // 할당된 메모리의 주소 반환
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));        
    PUT(HDRP(bp), PACK(size, 0));           // bp의 헤더에 alloc을 1에서 0으로 변경
    PUT(FTRP(bp), PACK(size, 0));           // bp의 푸터에 alloc을 1에서 0으로 변경
    coalesce(bp);                           // coalesce 함수로 현 블록 주변의 블록의 alloc값을 확인
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* allocate an even number of words to maintain alignment */
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;   // 작업공간은 더블워드(8byte) 정렬이 보장되야 하므로 워드의 갯수가 홀수면 짝수로 만들어서 WSIZE(4byte)를 곱한다. 
    if ((long)(bp=mem_sbrk(size))==-1)  return NULL;        // 만약 mem_sbrk에서 힙영역을 늘릴수 없다면 (void *)-1를 반환하는데 0xFFFFFFFF(-1)를 의미한다 (0xFFFFFFFFF+1=0)
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));                           // 가용 블록 헤더에 size와 alloc=0 값 넣음 (기존 에필로그 덮어씀)
    PUT(FTRP(bp), PACK(size, 0));                           // 가용 블록 푸터에 size와 alloc=0 값 넣음
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0, 1));                    // 가용 블록 푸터 뒤에 새로운 에필로그 넣음 
    /* Coalesce if the previous block was free */
    return coalesce(bp);                                    // coalesce 함수로 현 블록 주변의 블록(여기서는 이전 블록)의 alloc값을 확인
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     // 이전 블록의 푸터값을 보고 할당 여부를 확인
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     // 다음 블록의 헤더값을 보고 할당 여부를 확인
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc)    return bp;             // case 1 : 이전 블록과 다음 블록 모두 할당 되어 있는 경우 -> 블록끼리 연결하지 않음 
    else if (prev_alloc && !next_alloc)                     // case 2 : 이전 블록은 할당되어 있고 다음 블록은 할당 안된 경우
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));              // size = 현재 블록 + 다음 블록
        PUT(HDRP(bp), PACK(size, 0));                       // 현재 블록의 헤더에 size/0
        PUT(FTRP(bp), PACK(size, 0));                       // 다음 블록의 푸터에 size/0
    }
    else if (!prev_alloc && next_alloc)                     // case 3 : 이전 블록은 할당 안되어 있고 다음 블록은 할당된 경우
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));              // size = 현재 블록 + 이전 블록
        PUT(FTRP(bp), PACK(size, 0));                       // 현재 블록의 푸터에 size/0
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));            // 이전 블록의 헤더에 size/0
        bp = PREV_BLKP(bp);                                 // 연결된 블록의 포인터는 이전 블록의 포인터
    }
    else                                                    // case 4 : 이전 블록과 다음 블록 둘다 할당 안된 경우
    {                                                       // size = 이전 블록 + 현재 블록 + 다음 블록
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));            // 이전 블록의 헤더에 size/0
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));            // 다음 블록의 푸터에 size/0
        bp = PREV_BLKP(bp);                                 // 연결된 블록의 포인터는 이전 블록의 포인터
    }
    /* next_fit을 사용할 경우 초기 검색 할 포인터를 coalesce함수를 거친 포인터로 설정해준다 */
    #ifdef NEXT_FIT                                         
        search_p=bp;                                       
    #endif
    return bp;
}

static void *find_fit(size_t asize)
{
        /* Best-fit search */
    #ifdef BEST_FIT
        void *bp;
        void *small_p = NULL;
        for (bp=heap_listp; GET_SIZE(HDRP(bp))>0; bp=NEXT_BLKP(bp))
        {
            if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))                                     // 블록이 가용 블록이고 블록 사이즈가 asize를 수용할 수 있으면          
                if (small_p == NULL || (GET_SIZE(HDRP(small_p)) > GET_SIZE(HDRP(bp))))    small_p = bp;    // small_p 블록의 사이즈가 bp 블록의 사이즈보다 크다면 small_p=bp
        }
        if (small_p == NULL)  return NULL;
        else    return small_p;
    #else
        /* Next-fit search */
        #ifdef NEXT_FIT
            void *bp;
            for (bp = search_p; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp))               // 검색포인터부터 epilogue까지 블록을 순차적으로 검사하면서
                if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))  return bp;  // 블록이 가용 블록이고 블록 사이즈가 asize를 수용할 수 있으면 해당 블록 포인터 반환
            
            for (bp = heap_listp; bp<search_p; bp=NEXT_BLKP(bp))                        // epilogue까지 마땅한 블록을 찾지 못했으면 처음부터 검색포인터까지 블록을 순차적으로 검사하면서  
                if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))  return bp;  // 블록이 가용 블록이고 블록 사이즈가 asize를 수용할 수 있으면 해당 블록 포인터 반환

            // 적절한 가용블록을 찾지 못했다면 NULL 반환
            return NULL; 
        /* First-fit search */
        #else
            void *bp;
            for (bp = heap_listp; GET_SIZE(HDRP(bp))>0; bp = NEXT_BLKP(bp))             // prologue에서 eplilogue까지 블록을 순차적으로 검사하면서
                if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp))))  return bp;  // 블록이 가용 블록이고 블록 사이즈가 asize를 수용할 수 있으면 해당 블록 포인터 반환
            // 적절한 가용블록을 찾지 못했다면 NULL 반환
            return NULL; 
        #endif
    #endif
    
}
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));          // asize를 수용할 block의 사이즈 = csize
    if ((csize - asize) >= (2*DSIZE))           // csize >= asize + 블록의 최소 사이즈(16byte) 를 만족하면 
    {                                           // 블록을 asize와 csize-asize로 분할
        PUT(HDRP(bp),PACK(asize, 1));           // 첫 블록 헤더에 asize/1
        PUT(FTRP(bp),PACK(asize, 1));           // 첫 블록 푸터에 asize/1
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));  // 두번째 블록 헤더에 csize-asize/0
        PUT(FTRP(bp), PACK(csize - asize, 0));  // 두번째 블록 푸터에 csize-asize/0
    }
    else                                        // csize-asize를 뺀 나머지가 최소 사이즈보다 작으면
    {                                           // 블록을 분할하지 못함
        PUT(HDRP(bp), PACK(csize, 1));          // 블록 헤더에 csize/1
        PUT(FTRP(bp), PACK(csize, 1));          // 블록 푸터에 csize/1
    }
}











