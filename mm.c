/*
 * mm.c
 *
 * Name: Tianyi Wang
 * 
 * Note: remember to write a comment file
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 * Also, read malloclab.pdf carefully and in its entirety before beginning.
 *
 * 
 * 
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "mm.h"
#include "memlib.h"
#include "stree.h"



//define all the Macros
#define WSIZE 8
#define DSIZE 16
#define CHUNKSIZE (1 << 12)
size_t MAX(size_t x,size_t y){
    return ((x) > (y)? (x) : (y));
} 


//===========================================================================
//define some pointer cotrolers

//combine the size and alloc into one word header/footer
//#define PACK(size, alloc) ((size) | (alloc))
static size_t PACK(size_t size, char alloc){
    return ((size)|(alloc));
}

//read and write a word at address p
static size_t GET(void* p){
    return (* (size_t *) (p));
}
static void PUT(void* p, size_t val){
    (*(size_t *)(p) = (val));
}


//read the size and allocated indo from address p
static size_t GET_SIZE(void* p){
    return (GET(p) & (~0x7));
}
static size_t GET_ALLOC(void* p){
    return (GET(p) & 0x1);
}


//calculate the address of a word's header/footer
static void* HDRP(void* bp){
    return ((char *) (bp) - WSIZE);
}
static void* FTRP(void* bp){
    return ((char *) (bp) + GET_SIZE(HDRP(bp)));
}


//calculate block ptr bp, compute address of next and previous blocks
static void* NEXT_BLKP(void* bp){
    return (bp + GET_SIZE(HDRP(bp)) + DSIZE);
}

static char *heap_listp;
static int heap_size;
//=========================================================================


/*
 * If you want to enable your debugging output and heap checker code,
 * uncomment the following line. Be sure not to have debugging enabled
 * in your final submission.
 */
//#define DEBUG
#ifdef DEBUG
/* When debugging is enabled, the underlying functions get called */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated */
#define dbg_printf(...)
#define dbg_assert(...)
#endif /* DEBUG */

/* do not change the following! */
#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* DRIVER */

/* What is the correct alignment? */
#define ALIGNMENT 16

/* rounds up to the nearest multiple of ALIGNMENT */
static size_t align(size_t x)
{
    return ALIGNMENT * ((x+ALIGNMENT - 1)/ALIGNMENT);
}



//difine all the helper function
//static void *extend_heap(size_t words);
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static bool aligned(const void* p);



//extend the heap when the heap is initialized or not enough for malloc
//why we need this funciton when we have mm_sbrk?????
static void *extend_heap(size_t words){
    char *bp;
    size_t size;

    //initialize the words to matain alignment
    size = align(words);

    if ((long)(bp = mem_sbrk(size)) == -1){
        return NULL;
    }

    //initialize the headers and footers 
    //also create a new epilogue
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp) , PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp) - WSIZE) , PACK(0,1));

    //????????????????
    //return coalesce(bp);
    return bp;
}

// //this is the Function for coalescing the freed blocks 
// //with either front block or last one
// static void *coalesce(void *bp){
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
//     size_t size = GET_SIZE(HDRP(bp));

//     //case 1: when both pre and next block are not freed
//     if(prev_alloc && next_alloc) {
//         return bp;
//     }

//     //case 2: when next block is freed
//     else if (prev_alloc && !next_alloc){
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
//         PUT(HDRP(bp), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0));
//     }

//     //case 3: when pre block is freed
//     else if (!prev_alloc && next_alloc){
//         size += GET_SIZE(HDRP(PREV_BLKP(bp)));
//         PUT(FTRP(bp), PACK(size, 0));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         bp = PREV_BLKP(bp);
//     }

//     //case 4: when both pre and next are freed
//     else{
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
//         bp = PREV_BLKP(bp);
//     }
//     return bp;
// }


/*
 * Initialize: returns false on error, true on success.
 */
bool mm_init(void)
{
    heap_size = 0;
    /* IMPLEMENT THIS */
    if((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) -1){
        return false;
    }
    PUT(heap_listp ,0);                           //offset
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));  //prologue header 
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));  //prologue footer
    PUT(heap_listp + (3*WSIZE), PACK(0, 1));      //epilogue
    heap_listp += (4*WSIZE);
    
    //extend the empty heap with a free block of HUNKSIZE bytes
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return false;
    }
    return true;
}



/*
 * malloc
 */
void* malloc(size_t size)
{
    // printf("=========================\n");
    // printf("heap_listp at malloc: %p\n",heap_listp);
    //initialize the variable to record the adjusted block size 
    //if extension is needed, how much space
     size_t asize;
     void *bp ;

//    /* IMPLEMENT THIS */
//     // check if the size is 0 return NULL
//     if(size == 0){
//         return NULL;
//     }

//     //double word alignment ??????
//     if(size <= DSIZE){
//         asize = 2*DSIZE;
//     }
//     else{
//         asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
//     }

//     //search the free list for a fit
//     if((bp = find(asize)) != NULL){
//         place(bp,asize);
//         return bp;
//     }

//     //no fit get new memmory
//     extendsize = MAX(asize,CHUNKSIZE);
//     if((bp = extend_heap(extendsize/WSIZE)) == NULL)
//         return NULL;
//     place(bp,asize);
//     return bp;
    if(size == 0){
        return NULL;
    }
    //adjust the size to asize so it would be 16 byte aligned
    asize = align(size);


    //if the heap is not enough for malloc
    //extend them put
    if( ((size_t)(heap_listp + asize + DSIZE)) > (size_t)mem_heap_hi()){
        extend_heap(asize+DSIZE);
        // printf("block size:%ld\n",asize);
        // printf("Allocated size:%d\n",heap_size);
        // printf("Heap size %ld\n",mem_heapsize());
        // printf("Header address:%p\n Footer address:%p\n Header size: %ld\n Header flag: %ld\n", HDRP(heap_listp),FTRP(heap_listp + asize - DSIZE),GET_SIZE(heap_listp - WSIZE), GET_ALLOC(heap_listp- WSIZE));
        // printf("list pointer%p\n",heap_listp);
        // printf("Malloc Pointer: %p\n",bp);
        // printf("NEXT POINTER:%p\n",heap_listp);
        // printf("=========================\n");
    }

    PUT(HDRP(heap_listp), PACK(asize,1));
    PUT(FTRP(heap_listp), PACK(asize,1));
    bp = heap_listp;
    heap_listp += (asize + DSIZE);
    return bp;
}


/*
 * free
 */
void free(void* ptr)
{
    /* IMPLEMENT THIS */
    //store the size of the current block
    size_t size = GET_SIZE(ptr - WSIZE);   

    //write the size and update the flag of status to the current block
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    
    //coalesce if we can
    //coalesce(ptr);
}


/*
 * realloc
 */
void* realloc(void* oldptr, size_t size)
{
    /* IMPLEMENT THIS */
    char* newp;
    newp = malloc(size);
    memcpy(newp,oldptr,size);
    free(oldptr);
    return newp;
}


/*
 * calloc
 * This function is not tested by mdriver, and has been implemented for you.
 * 
 */
void* calloc(size_t nmemb, size_t size)
{
    void* ptr;
    size *= nmemb;
    ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

/*
 * Returns whether the pointer is in the heap.
 * May be useful for debugging.
 */
static bool in_heap(const void* p)
{
    return p <= mem_heap_hi() && p >= mem_heap_lo();
}


/*
 * Returns whether the pointer is aligned.
 * May be useful for debugging.
 */
static bool aligned(const void* p)
{
    size_t ip = (size_t) p;
    return align(ip) == ip;
}

/*
 * mm_checkheap
 */
bool mm_checkheap(int lineno)
{
#ifdef DEBUG
    /* Write code to check heap invariants here */
    /* IMPLEMENT THIS */
    //check the heap size
    // char* heaps = mem_heap_lo();
    // heaps = heaps + DSIZE;
    // int block = 0;
    // while (heaps < (char*)mem_heap_hi())
    // {
    //     printf("================================\n");
    //     printf("\t\tBlock Number: %d\n",blockNum);
    //     printf("Header address:%p\nFooter address:%p\nHeader size: %ld\nHeader flag: %ld\n", HDRP(heap_listp),FTRP(heap_listp),GET_SIZE(heap_listp - WSIZE), GET_ALLOC(heap_listp- WSIZE));
    //     printf("\t\tEnd of Block%d\n",blockNum);
    //     printf("================================\n");
    //     block ++;
    //     heaps = NEXT_BLKP(heaps);
    // }
    
    printf("================================\n");
    if(in_heap(heap_listp)){
        printf("This block is in the heap\n");
    }
    else{
        printf("This block is outside the heap!!!!!\n");
    }
    printf("Block Info: \n");
    printf("This is %d Block\n",blockNum);
    printf("Heap pointer: %p\n",heap_listp);
    //printf("Malloc return Pointer: %p\n",bp);
    if(heap_listp == mem_heap_lo() + 4*WSIZE){
        printf("This is the first block in the heap !!!\n");
    }
    else if((size_t)heap_listp == (size_t)(mem_heap_hi() - DSIZE - GET_SIZE(heap_listp))){
        printf("This is the last block in the heap !!!\n");
    }
    //printf("assigned size:%ld\n",asize);
    printf("Allocated size:%ld\n",GET_SIZE(heap_listp - WSIZE));
    printf("Current heap size before extend: %d\n",currentHeapSize);
    printf("Total heap size: %ld\n",mem_heapsize());
    printf("Header address:%p\nFooter address:%p\nHeader size: %ld\nHeader flag: %ld\n", HDRP(heap_listp),FTRP(heap_listp),GET_SIZE(heap_listp - WSIZE), GET_ALLOC(heap_listp- WSIZE));
    printf("NEXT Block POINTER:%p\n",NEXT_BLKP(heap_listp));
    printf("Next Block Header ADD: %p\n",HDRP(NEXT_BLKP(heap_listp)));
    printf("ALLOC:%ld\n",GET_ALLOC(prologueH));
    printf("ALLOC:%ld\n",GET_ALLOC(prologueF));
    printf("prolugueH: %p\n",prologueH);
    printf("prolugueF: %p\n",prologueF);
    printf("==============================\n\n\n");
//heap_listp + GET_SIZE(heap_listp - WSIZE) - DSIZE

#endif /* DEBUG */
    return true;
}