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
    "Yichen Shen",
    /* First member's email address */
    "shenyichen105@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

#define DEBUG 0

#ifdef DEBUG
#define CHECKHEAP() printf("%s\n", __func__); mm_checkheap();
#define CHECKLIST() printf("%s\n", __func__); mm_checklist();
#define PRINT_PTR(ptr) printf("%p\n", (void *) ptr);
#endif

//define a node for explicit list
typedef struct linked_list_node
{
    struct linked_list_node *prev;
    struct linked_list_node *next;

}linked_list_node;


/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define NODE_SIZE  (sizeof(linked_list_node))
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE       4       /* Word and header/footer size (bytes) */
#define DSIZE       8       /* Double word size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */
#define MAX_HEAP (20*(1<<20))

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc))

/* Read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val)  (*(unsigned int *) (p) = val)

/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - NODE_SIZE - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE - NODE_SIZE)
#define NRP(bp) ((linked_list_node *)((char *)(bp) - NODE_SIZE))

/* Given a node, compute address of bp */
#define NP_TO_BP(np) ((char *)(np) + NODE_SIZE)
#define HD_TO_BP(hdrp) ((char *)(hdrp) + NODE_SIZE + WSIZE)
#define FT_TO_BP(ftrp) ((char *)(ftrp) + NODE_SIZE + DSIZE - GET_SIZE(ftrp))

/*compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - 2*DSIZE)))


//per csapp text book, it is pointing to the second block of prologue blocks
static char *heap_listp;
/*head for linked list*/
linked_list_node * empty_blocks_head;

/*calculate address offset of a pointer relative to start of heap (after prologue) in decimal*/
int address_offset(void* p){
    if (!p){
        return -999;
    }
    int offset = (int)((unsigned int) p) - ((unsigned int) (heap_listp + WSIZE));
    return offset;
}

/*heap checker print all the blocks, start from the prologue*/
void mm_checkheap(){
    char* prev_ptr;
    char* next_ptr;

    //prologue
    char* cur_hd_ptr = (char *) heap_listp;
    //char* cur_blk_ptr;

    printf("prologue block at address: %d \n", address_offset(cur_hd_ptr));

    //heap area
    cur_hd_ptr += WSIZE;
    //cur_blk_ptr = HD_TO_BP(cur_hd_ptr);

    unsigned cur_size = GET_SIZE(cur_hd_ptr);
    unsigned cur_status = GET_ALLOC(cur_hd_ptr);
    int count = 0;
    while (cur_size > 0){
        printf("block %d at address %p: size %u status %u ", count, cur_hd_ptr, cur_size, cur_status);

        prev_ptr = PREV_BLKP(HD_TO_BP(cur_hd_ptr));
        next_ptr = NEXT_BLKP(HD_TO_BP(cur_hd_ptr));

        printf("prev_block addr: %d; next_block addr: %d \n", address_offset(prev_ptr), address_offset(next_ptr));

        cur_hd_ptr = cur_hd_ptr + cur_size;
        //cur_blk_ptr = HD_TO_BP(cur_hd_ptr);
        count++;
        cur_size = GET_SIZE(cur_hd_ptr);
        cur_status = GET_ALLOC(cur_hd_ptr);
    }
    printf("epilogue block at address: %d \n", address_offset(cur_hd_ptr));
    printf("current effective heap size: %u \n", mem_heapsize() - 2 * DSIZE - NODE_SIZE);
    printf("heap high address: %d \n", address_offset(mem_heap_hi()));
    printf("\n");
}


/*heap checker print all the free list, start from the head*/
void mm_checklist(){

    linked_list_node *prev_node = empty_blocks_head;
    linked_list_node *cur_node;

    printf("block head at %d\n",address_offset(empty_blocks_head));

    char* cur_blk_ptr;

    while((cur_node = prev_node->next)){
        cur_blk_ptr = NP_TO_BP(cur_node);
        size_t cur_size = GET_SIZE(HDRP(cur_blk_ptr));
        size_t cur_status = GET_ALLOC(HDRP(cur_blk_ptr));
        printf("free block at address %d: size %u status %u  prev %d next %d \n",
            address_offset(cur_blk_ptr), cur_size, cur_status, address_offset(cur_node->prev),
            address_offset(cur_node->next));
        prev_node = cur_node;
    }
}


/*insert a freed block into list*/
linked_list_node* list_insert(linked_list_node *head, linked_list_node *np){
    //*bp must an initialized/known address
    if (!np){
        return NULL;
    }
    np -> prev = NULL;
    np -> next = NULL;
    //always insert into first (right after head)
    if (head->next == NULL){
        //empty list
        head->next = np;
        np->prev = head;
    }else{
        linked_list_node* old_next = head -> next;
        head -> next = np;
        np -> prev = head;
        np->next = old_next;
        old_next->prev = np;
    }
    return np;
}

/*delete a allocated block into list*/
linked_list_node* list_delete(linked_list_node *np){
    //*bp must be a node have been inserted into the list (must have a prev)
    if ((!np) || !(np->prev)){
        return NULL;
    }

    linked_list_node* old_prev;
    linked_list_node* old_next;

    if (!np->next){
        //bp is the end of list
        old_prev = np -> prev;
        np->prev = NULL;
        old_prev -> next = NULL;
    }else{
        old_prev = np -> prev;
        old_next = np -> next;
        np->prev = NULL;
        np->next = NULL;
        old_prev -> next = old_next;
        old_next -> prev = old_prev;
    }
    return np;
}

static void *coalesce(void *bp)
{
    //CHECKHEAP();
    //printf("%d\n", address_offset(bp));


    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    linked_list_node* cur;
    linked_list_node* next;

    if (prev_alloc && next_alloc) {
        return bp;
    }

    else if (prev_alloc && !next_alloc) {
        //delete next free block from the list
        next = (linked_list_node*)NRP(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        list_delete(next);

        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size,0));
    }

    else if (!prev_alloc && next_alloc) {

        //delete current free block from the list
        cur = (linked_list_node*)NRP(bp);
        list_delete(cur);

        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    else {
        //delete current free block from the list
        cur = (linked_list_node*)NRP(bp);
        list_delete(cur);
        //delete next free block from the list
        next = (linked_list_node*)NRP(NEXT_BLKP(bp));
        list_delete(next);

        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

static void *extend_heap(size_t words){
    linked_list_node *np;
    void *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    /* allocate at least 6 words, two for linked list and two for footer/epilogue*/
    if (size <= 0)
        return NULL;
    if (size < 6 * WSIZE){
        size = 6 * WSIZE;
    }
    if ((long)(np = mem_sbrk(size)) == -1)
        return NULL;
    //np is the start of linked list
    //bp is the start of the block
    bp = (void *) NP_TO_BP(np);
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0)); /* Free block header */
    PUT(FTRP(bp), PACK(size, 0)); /* Free block footer */

    //insert the free block to the free block list
    list_insert(empty_blocks_head,np);

    //CHECKLIST();

    PUT(FTRP(bp) + WSIZE, PACK(0, 1)); /* New epilogue header */
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{

    mem_deinit();
    mem_init();
    mem_reset_brk();

    if (DEBUG) printf("____________init.....______________\n");

    if ((heap_listp = mem_sbrk(4*WSIZE + NODE_SIZE)) == (void *)-1) return -1;

    PUT(heap_listp, 0);
    //prologue block header
    PUT(heap_listp + WSIZE, PACK(DSIZE + NODE_SIZE, 1));
    //put linked list node inside the prologue
    empty_blocks_head = (linked_list_node *) (heap_listp + 2*WSIZE);

    empty_blocks_head -> prev = NULL;
    empty_blocks_head -> next = NULL;

    PUT(heap_listp + (2*WSIZE + NODE_SIZE), PACK(DSIZE + NODE_SIZE, 1));
    //epilogue block
    PUT(heap_listp + (3*WSIZE + NODE_SIZE), PACK(0, 1));

    heap_listp += DSIZE + NODE_SIZE;
    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}



void *find_fit(size_t size){
    linked_list_node *empty_blk;
    //CHECKLIST();

    if (!(empty_blocks_head) || (!(empty_blk = empty_blocks_head -> next))){
        return NULL;
    }
    //record the size and location of the first free block
    size_t best_blk_size = 2 * mem_heapsize() + 1; //a very large number, any qualify fit will replace this
    size_t cur_blk_size;
    void * best_bp = NULL;
    //CHECKLIST();

    /* traverse all the linked list to get a best fit*/
    while (empty_blk){
        cur_blk_size = GET_SIZE(HDRP(NP_TO_BP(empty_blk)));
        //printf("block size:%u size:%u\n", cur_blk_size, size);
        if (cur_blk_size >= size){
            //check if better than current best fit
            if ((cur_blk_size - size) < (best_blk_size - size)){
                best_blk_size = cur_blk_size;
                best_bp = (void *) NP_TO_BP(empty_blk);
            }
        }
        empty_blk = empty_blk ->next;
    }
    if (DEBUG && best_bp) printf("find_fit of %u bytes with %u bytes block at %d\n", size, best_blk_size, address_offset(best_bp));
    return best_bp;
}


void* allocate_block(void* bp, size_t size){
    if (DEBUG)
        printf("allocate_block %u bytes at %d\n", size, address_offset(bp));

    if (!bp){
        return NULL;
    }
    //no splitting is needed, return the whole block
    PUT(HDRP(bp),PACK(size, 1));
    PUT(FTRP(bp),PACK(size, 1));
    //delete block from linked list
    list_delete(NRP(bp));
    return bp;
}

void *deallocate_block(void* bp, size_t size){
    if (DEBUG)
        printf("deallocate_block %u bytes at %d\n", size, address_offset(bp));

    if (!bp){
        return NULL;
    }
    //no splitting is needed, return the whole block
    PUT(HDRP(bp),PACK(size, 0));
    PUT(FTRP(bp),PACK(size, 0));
    //delete block from linked list
    linked_list_node *np = NRP(bp);
    list_insert(empty_blocks_head, np);
    //CHECKLIST();
    //CHECKHEAP();
    return bp;
}

void *split_block(void* bp, size_t alloc_size){

    if (DEBUG)
        printf("split_block %u bytes at %d\n", alloc_size, address_offset(bp));

    if (!bp || GET_ALLOC(HDRP(bp))){
        return NULL;
    }
    //assume here size is already aligned
    char* cur_head = HDRP(bp);

    size_t original_size = GET_SIZE(cur_head);
    size_t remainder_size = original_size - alloc_size;

    char* remainder_bp = bp + alloc_size;
    allocate_block(bp, alloc_size);
    deallocate_block((void *)remainder_bp, remainder_size);
    return bp;
}


void *place(void* bp, size_t size){
    if (DEBUG)
        printf("place: place %u bytes at block %d\n", size, address_offset(bp));

    if (!bp || GET_ALLOC(HDRP(bp))){
        return NULL;
    }
    //determine if splitting is needed
    size_t bsize = GET_SIZE(HDRP(bp));
    if (bsize - size < 3 * DSIZE){
        bp = allocate_block(bp, bsize);
    }else{
        //splitting is needed
        bp = split_block(bp, size);
    }
    return bp;
}

/*helper function to calculate allocated size based on payload*/

size_t get_alloc_size(size_t size){
    /* pad the block to meet the alignment requirement*/
    size_t asize;
    if (size <= DSIZE)
        asize = 3*DSIZE;
    else
        asize = DSIZE * ((size + (2*DSIZE) + (DSIZE-1)) / DSIZE);
    return asize;
}


/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */


void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    char *bp;

    //CHECKLIST();

    /* ignore spurious size*/
    if (size == 0)
        return NULL;

    asize = get_alloc_size(size);

    if (DEBUG) printf("mmalloc: allocate %u bytes with %u bytes payload\n", asize, size);

    /* Search the free list for a fit */
    /* No fit found. Get more memory (2X current heap size) and place the block */
    bp = find_fit(asize);

    while (bp == NULL && mem_heapsize() <= MAX_HEAP){
        extendsize = mem_heapsize() - 2 * DSIZE - NODE_SIZE;
        if (!(extend_heap(extendsize/WSIZE))) return NULL;
        bp = find_fit(asize);
    }

    bp = place(bp, asize);
    return bp;
}

/*
 * mm_free
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    deallocate_block(bp, size);
    coalesce(bp);
}



/*
 * mm_realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t asize;
    size_t old_size;
    size_t payload;

    if (!size){
        mm_free(ptr);
        return NULL;
    }
    if (!ptr){
        return mm_malloc(size);
    }

    asize = get_alloc_size(size);
    //shoud check if the pointer is valid
    old_size = GET_SIZE(HDRP(ptr));

    if (old_size == asize){
        return ptr;
    }else if(old_size > asize){
        //free the block then place the smaller block into freed block
        mm_free(ptr);
        ptr = place(ptr, asize);
    }else{
        char* cur_block = (char *)ptr;
        mm_free(cur_block);

        payload = old_size - 16;
        char temp_storage[payload];
        //has to copy out the bytes first before mmalloc
        if (!memcpy(temp_storage, ptr, payload)){
            printf("mm_realloc: memcpy error\n");
            exit(1);
        }
        void* new_ptr = mm_malloc(size);

        if (!memmove(new_ptr, temp_storage, payload)){
            printf("mm_realloc: memcpy error\n");
            exit(1);
        }

        if (DEBUG) printf("copyed %d bytes from %d to %d\n", payload, address_offset(ptr), address_offset(new_ptr));

        ptr = new_ptr;

    }
    return ptr;
}














