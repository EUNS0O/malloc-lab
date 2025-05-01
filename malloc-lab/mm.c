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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
//메모리 할당 시 메모리 블록의 크기가 8byte 배수로 할당
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
//주어진 size값을 ALIGNMENT(8byte)로 정렬된 값으로 변환
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
//size_t 타입의 크기를 정렬된 크기로 정의한 매크로.
//sizeof(size_t)로 구한 size_t의 크기를 ALIGN()을 사용하여 정렬된 크기로 변환함.
//size_t 타입의 크기를 8바이트 단위로 정렬하는 데 사용됨.

//implicit free list 
#define WSIZE 4 //Word Size. 4바이트(32비트). 블록 헤더(Header)와 푸터(Footer)의 크기
#define DSIZE 8 //Double World Size. 8바이트. 64비트 시스템에서는 블록 최소 크기.
#define CHUNKSIZE (1<<12) //초기 힙을 확장할 때 2^12 = 4096 바이트만큼 늘림(4KB). 페이지 크기와 맞추기도 함.

#define MAX(x,y) ((x)>(y)? (x):(y)) //x와 y 중에서 더 큰 값을 반환하는 매크로 함수.

#define PACK(size, alloc) ((size)|(alloc))
//블록 정보 다루기 (헤더/푸터 읽고 쓰기)
//size와 alloc 상태를 하나의 4바이트 정수로 합쳐줌
//(할당된 블록인지 표시할 수 있어야 하니까 마지막 1bit를 alloc으로 사용)

#define GET(p) (*(unsigned int *)(p)) //읽어옴.포인터 p가 가리키는 위치에서 4바이트(unsigned int)를 읽음.
#define PUT(p,val) (*(unsigned int*)(p)=(val)) //저장.포인터 p가 가리키는 위치에 4바이트(val) 저장함.

#define GET_SIZE(p) (GET(p) & ~0x7) // p가 가리키는 4바이트에서 상위 비트 (크기 정보)만 추출(&~0x7). 
//블록 크기는 항상 8의 배수-> 하위 3비트(0b111) 버림.
#define GET_ALLOC(p) (GET(p) & 0x1) //p가 가리키는 4바이트에서 가장 마지막 1비트만 읽음(0이면 free, 1이면 allocated).

#define HDRP(bp) ((char *)(bp) - WSIZE) //현재 블록 포인터(bp)(payload 시작 주소) 기준으로 헤더의 주소를 계산 (bp - 4바이트)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //현재 블록 포인터(bp)(payload 시작 주소) 기준으로 푸터의 주소를 계산.(헤더에서 블록 크기만큼 이동 후 -8바이트)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //현재 블록 포인터에서 다음블록(bp)의 시작 주소를 계산
//현재 블록의 헤더를 보고 블록 전체 크기를 알아낸 다음, 그만큼 bp에서 앞으로 이동하면 다음 블록 시작 주소가 나옴.
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //현재 블록 포인터에서 이전 블록의 시작 주소를 계산
//현재 블록 앞에 있는 블록의 footer를 ㅇ릭어서 "이전 블록의 크기"를 알아낸 후, 그만큼 bp에서 뒤로 이동하면 이전 블록 시작 주소가 나옴.

/*
 * mm_init - initialize the malloc package.
 */
//힙 영역의 시작 위치 설정.
//할당기를 초기화(필요한 데이터 구조와 환경을 설정.힙 공간을 준비.)
//가용리스트 설정
//빈 공간 추적 시 필요한 데이터 초기화
//초기화된 힙을 바탕으로 
//메모리 할당에 필요한 데이터 구조(가용리스트,헤더/풋터 구조)설정
//메모리 정렬 및 블록 크기 설정
//성공하면 0 아니면 -1을 리턴. 

void *heap_listp; //힙의 시작 주소를 가리킬 포인터 선언
 
int mm_init(void)
{
    if((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) //Create the initial empty heap
        return -1;
        //힙 영역을 4*WSIZE만큼 확장
        //WSIZE는 단어 크기. 64bit system에서는 8바이트.
        //힙의 시작 주소가 heap_listp에 저장.
    PUT(heap_listp, 0 ); //heap_listp가 가리키는 주소에 0을 저장. 가용 블록을 나타내는 헤더로 사용
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); //블록 크기와 할당된 상태를 저장하는 매크로
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE,1)); //프롤로그 헤더를 설정. 메모리 할당 시스템의 시작 부분. 할당된 크기와 상태를 저장.
    PUT(heap_listp + (3*WSIZE), PACK(0,1));  //프롤로그 풋터를 설정. 0이 저장됨, 끝 블록을 표시.
    heap_listp += (2*WSIZE); //heap_listp를 2*WSIZE만큼 이동시켜 에필로그 헤더를 설정. 가용 리스트의 끝을 표시함. 힙의 끝을 나타내는 특수한 블록.


    if(extend_heap(CHUNKSIZE/WSIZE) == NULL) //힙을 확장하는 함수. CHUNKSIZE/WSIZE 만큼 힙을 확장.
    //CHUNKSIZE는 힙의 확장 크기. (정적 크기).할당 가능한 메모리 크기
    //WSIZE는 단어 크기. 힙을 확장할 때 필요한 메모리 크기를 맞추기 위해 나누어줌.
        return -1; //초기화 실패(extend_heap함수가 NULL반환)

    return 0;
}

static void *extend_heap(size_t words)//현재 힙이 부족해서 새로운 메모리를 확장할 때 호출
{ //(리눅스 시스템 호출)mem_sbrk로 메모리 확보, 이를 가용 블록으로 초기화한 후 병합(coalesce)
    char *bp;
    size_t size;

    /*Allocate an even number of words to maintain alignment*/
    size = (words%2) ? (words+1) * WSIZE : words * WSIZE;
    //(힙은 항상 8바이트(DSIZE)정렬을 유지해야 함)
    //요청한 단어 수(words)가 홀수면 짝수로 맞춰서 8바이트 단위 정렬을 보장
    //(ex. words = 7-> size = 8*4 = 32)

    if ((long)(bp = mem_sbrk(size)) == -1) 
    //mem_sbrk(size)로 힙 영역을 size만큼 확장.
    //실패하면 -1 반환하고 NULL로 처리
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); 
    PUT(FTRP(bp), PACK(size, 0));
    //새로 확장된 영역의 헤더와 푸터 초기화
    //PACK(size,0)->블록 크기와 alloc = 0(즉, free block)
    //bp는 payload 시작점, HDRP(bp)는 헤더 위치, FTRP(bp)는 푸터 위치

    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1));
    //힙의 새로운 끝(epilogue block)을 설정
    //크기 0, 할당됨(1) 상태의 블록을 넣어 종료 마커로 사용.
    /*Coalesce if the previous block was free*/
    return coalesce(bp);
    //새로 생성된 free block을 이전 블록과 병합(coalesce) 시도
    //(외부 단편화 줄이기)
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
//mm_malloc - brk 포인터를 증가시켜 블록을 할당하다. 항상 정렬의 배수인 크기의 블록을 할당한다.

//void *mm_malloc(size_t size)
//{
//    int newsize = ALIGN(size + SIZE_T_SIZE);
    //메모리 블록이 8바이트 경계로 정렬되도록 보장하는 작업
    //사용자가 요청한 메모리 크기 + 헤더 크기(헤더에 저장할 정보를 위한 공간)
    //사용자 요청 메모리크기와 헤더 크기를 합친 후 이 값을 정렬.
    //(이 과정에서 8바이트 경계에 맞게 메모리 블록의 크기가 조정됨.)

//    void *p = mem_sbrk(newsize);
    //정렬된 메모리 크기(newsize)만큼 힙 메모리의 끝을 확장

//    if (p == (void *)-1)//만약 mem_sbrk가 실패하면 p는 (void*)-1이 됨.
//        return NULL;
//    else
//    {
//        *(size_t *)p = size;
        //할당된 메모리 블록의 시작 주소에 요청된 메모리 크기(size)를 헤더로 기록
        //(free나 realloc에서 메모리 블록 크기를 확인하는 데 사용됨)

//        return (void *)((char *)p + SIZE_T_SIZE);
        //p는 메모리 블록의 시작 주소를 가리킴.
        //이 주소는 헤더를 포함하고 있음->사용자가 원하는 데이터 영역이 아님.
        //-> p+SIZE_T_SIZE만큼 이동함.
        //p를 char*로 형변환한 후, SIZE_T_SIZE만큼 이동하여 데이터 영역의 시작 주소를 반환함.
    
        //최종적으로 실제로 사용자가 요청한 메모리 공간의 시작 주소를 반환함.
//    }
//}


void *mm_malloc(size_t size)//size: 사용자가 요청한 바이트 수. 반환값: 실제 메모리 블록의 payload 시작 주소(헤더는 숨겨져 있음)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    if(size == 0) //크기가 0이면 아무 것도 할당할 필요가 없으므로 NULL 반환
        return NULL;

    if(size <= DSIZE) //작은 요청은 최소 블록 크기(16 bytes)로 할당(헤더+푸터+정렬을 위해 필요)
        asize = 2*DSIZE;
    else
        asize = DSIZE *((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    // size + (DSIZE) : 헤더/푸터를 고려한 전체 블록 크기
    // + (DSIZE - 1) : 8의 배수로 올림 처리
    // / DSIZE * DSIZE : 정렬된 크기로 반올림(8바이트 align)
    // ===> asize는 최소 블록 단위로 정렬된 크기

    if((bp = find_fit(asize)) != NULL){ //내부 가용 리스트에서 asize 크기 이상의 블록을 찾음
        place(bp, asize); //있으면 그 위치에 블록을 할당(place)하고 바로 반환
        return bp;
    }

    // 만약 맞는 블록이 없다면
    extendsize = MAX(asize, CHUNKSIZE); // asize와 CHUNKSIZE 중 더 큰 값만큼 힙을 확장
    //(너무 자주 확장하지 않도록 최소 CHUNKSIZE만큼 늘리는 것)

    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
    //실제 힙 확장
        return NULL;
    // 실패 시 NULL 반환

    place(bp, asize);
    //확장된 공간을 블록으로 할당(place)

    return bp;   
    //사용자에게 데이터 영역 주소 반환
}


/*
 * mm_free - Freeing a block does nothing.
 */
//mm_free - 블록을 해제하는 작업은 아무것도 하지 않는다.
void mm_free(void *ptr)// ptr: 사용자가 할당받았던 메모리의 payload시작 주소
{
    size_t size = GET_SIZE(HDRP(ptr));
    //ptr은 payload영역, HDRP(ptr)은 헤더 위치
    //거기서 블록 전체 크기(헤더+payload+푸터)를 얻어옴.

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr),  PACK(size, 0));
    //블록의 헤더/푸터를 가용상태(alloc = 0)로 설정
    //PACK(size, 0)은 크기 + 할당 여부를 하나의 4바이트 값으로 합친 것
    //이걸 각각 헤더 위치, 푸터 위치에 저장

    /*Coalesce if the previous block was free*/
    coalesce(ptr);
    //인접한 블록(이전 블록, 다음 블록)이 가용 상태 -> 하나로 병합
    //외부 단편화 줄임.
}


static void *coalesce(void *bp) // bp: 가용 상태로 바뀐 블록의 payload 시작 주소. 반환값: 병합된(또는 그대로 유지된) 가용 블록의 시작 주소
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); //이전 블록이 할당되었는지 확인(푸터에서 확인)
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); //다음 블록이 할당되었는지 확인(헤더에서 확인)
    size_t size = GET_SIZE(HDRP(bp)); //현재 블록의 크기

    if(prev_alloc && next_alloc){ //case 1:  앞 할당o & 뒤 할당o
        return bp; //병합 가능 블록 없음 -> 그대로유지
    }

    else if(prev_alloc && !next_alloc){ //case 2: 앞 할당o & 뒤 할당x(가용)
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); 
        PUT(HDRP(bp), PACK(size, 0)); //헤더: 현재 블록 
        PUT(FTRP(bp), PACK(size, 0)); //푸터: 다음 블록
        //(현재 블록과 다음 블록 병합)
    }

    else if(!prev_alloc && next_alloc){ //case 3: 앞 할당x & 뒤 할당o
        size += GET_SIZE(HDRP(PREV_BLKP(bp))); 
        PUT(FTRP(bp), PACK(size, 0)); //헤더: 이전 블록
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0)); //푸터: 현재 블록
        bp = PREV_BLKP(bp); //bp를 이전 블록으로 이동
        //(현재 블록과 이전 블록 병합)

    }
    else{ //case 4: 앞 할당 x & 뒤 할당 x
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp))); 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); //헤더: 이전 블록
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); //푸터: 다음 블록
        bp = PREV_BLKP(bp);//bp: 이전 블록
        //세 블록(이전,현재,다음)을 모두 병합
    }
    return bp;
    //(병합된 블록의 시작 주소(bp)를 반환해야, free list에 넣거나 재활용할 수 있음.)

}


 



/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
//mm_realloc - mm_malloc과 mm_free를 이용해 간단하게 구현된 함수
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


static void *find_fit(size_t asize)
{

    void *bp;//현재 검사 중인 블록의 payload 주소(block pointer)

    //힙의 시작 지점부터 끝까지 순회
    for(bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)){
        //힙 시작 위치에서 순회 시작
        //epilogue 블록(크기 0)이 나오기 전까지 순회

        
        if(!GET_ALLOC(HDRP(bp))&&(asize <= GET_SIZE(HDERP(bp)))){
            //블록이 free상태인지 확인 & 요청 크기보다 블록 크기가 같거나 큰지 확인
            return bp;// 조건을 만족하는 블록 반환
        
        }
    }
    //적절한 블록을 찾지 못한 경우
    return NULL;
}


static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));// 현재 블록의 전체 크기

    //현재 블록을 asize로 나눌 수 있다면 -> 분할
    if((csize - asize) >= (2*DSIZE)){//(남은 공간이 최소 블록 크기 이상이면 분할 가능)
        ///분할 가능한 경우
        
        PUT(HDRP(bp), PACK(asize, 1)); //앞쪽 asize만큼 헤더: 할당 표시
        PUT(FTRP(bp), PACK(asize, 1)); // 푸터도 할당 표시

        //뒤쪽에 남은 공간을 새로운 가용 블록으로 설정
        bp = NEXT_BLKP(bp); //분할 후 가용 블록을 가리킬 포인터로 이동
        PUT(HDRP(bp), PACK(csize - asize, 0)); //나머지 공간의 헤더를 free상태로 기록
        PUT(FTRP(bp), PACK(csize - asize, 0)); //푸터도 free상태로 기록
    }
    else{
        //분할 불가능.딱 맞거나 남는 공간이 너무 작음->분할하지 않고 전체 사용
        PUT(HDRP(bp), PACK(csize, 1)); //전체를 할당된 블록으로 설정
        PUT(FTRP(bp), PACK(csize, 1)); 
    }
}

//realloc : 기존에 할당된 메모리(ptr)의 크기를 size로 변경
//가능한 경우 기존 블록 재사용, 필요 시 새 블록 할당 후 내용 복사 및 기존 블록 해제
void *mm_realloc(void *ptr, size_t size){
    if(ptr == NULL)//ptr이 NULL이란건, 처음부터 할당되지 않은 상태라는 뜻
        return mm_malloc(size); //단순히 malloc(size)처럼 새로 할당해주면 됨.

    if(size == 0){//요청 크기가 0이다->할당을 취소하고 싶다.와 같은 말
        mm_free(ptr);//->기존 메모리 블록을 free하고 NULL 반환
        return NULL;
    }

    size_t oldsize = GET_SIZE(HDRP(ptr));//현재 ptr의 헤더에서 블록의 전체 크기를 읽어옴.(헤더/푸터/패딩/payload 포함)
    size_t newsize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE); //새로 요청한 size를 8바이트 단위로 정렬
    //+ DSIZE는 헤더/푸터를 고려한 여유 공간
    // +(DSIZE - 1) 후 /DSIZE * DSIZE는 올림 정렬(ceil)

    if (newsize <= oldsize){ //새로 요청한 크기가 현재 블록보다 작거나 같으면 기존 메모리 그대로 재사용할 수 있음.
        return ptr; //복사없이 그대로 반환
    } 
    else{
        void *newptr = mm_malloc(size); //기존 블록이 너무 작아 새로 공간이 필요하면 새 블록 할당(malloc호출)
        if(newptr == NULL)return NULL; //실패하면 NULL반환

        memcpy(newptr, ptr, oldsize - DSIZE); //기존 블록의 내용을 새 블록으로 복사함.
        //oldsize - DSIZE: 전체 블록 크기에서 헤더와 푸터 제외.(실제 payload만 복사)
        mm_free(ptr); //기존 블록을 해제
        return newptr;  //새 블록 포인터를 반환
    }
}
 