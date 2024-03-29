// #include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stddef.h>


/*

    Block:

    ----------------------------- 

    front block header (4 bytes)  

    ----------------------------- 

            payload 

    ----------------------------- 

     back block header (4 bytes)
    
    ------------------------------
*/

typedef struct block_header_t {
    uint32_t status : 3;
    uint32_t size : 29;
}block_header_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
// min payload size is 1
#define MIN_BLOCK_SIZE BLOCK_HEADER_SIZE * 2 + 1


#define PAGE_SIZE 0x1000
#define HEAP_START 0x50000

#define MMAP_PROT PROT_READ | PROT_WRITE
#define MMAP_FLAGS MAP_PRIVATE | MAP_ANONYMOUS

#define ALLOCATED 1
#define FREE 0 

#define GET_PAYLOAD_SIZE(block_header) (block_header->size - BLOCK_HEADER_SIZE * 2)
#define IS_ALLOCATED(block_header) (block_header->status & ALLOCATED) 

#define EXPECTED_BLOCK_SIZE(requested_size) (requested_size + BLOCK_HEADER_SIZE * 2)
#define IS_FITTING(block_header, requested_size) (requested_size <= GET_PAYLOAD_SIZE(block_header) && block_header->size - EXPECTED_BLOCK_SIZE(requested_size) >= MIN_BLOCK_SIZE)

#define IS_VALID_BLOCK(block_header, requested_size) (!IS_ALLOCATED(block_header) && IS_FITTING(block_header, requested_size))

#define JMP_TO_NEXT_BH(bh) ( (block_header_t*) ( (void*) bh + GET_PAYLOAD_SIZE(bh) + BLOCK_HEADER_SIZE) )

#define SET_ALLOCATED(block_header) (block_header->status = ALLOCATED)
#define SET_FREE(block_header) (block_header->status = FREE)

static block_header_t* head = NULL;
static block_header_t* tail = NULL;

static uint32_t allocs = 0;
static uint32_t page_allocs = 0;

static block_header_t* create_block(uint32_t requested_size, void* dest);

static block_header_t create_block_header(uint16_t size, bool is_allocated) {
    block_header_t bh;

    bh.size = size;
    bh.status = is_allocated;
    return bh;
}

static block_header_t* create_free_list(void* base_address) {
    // returns the entry point to a new free 4kb block of memory 
    int i;
    void* ptr = base_address;

    // one big free block that holds head and tail.
    // 2*block header will be added by create block. save space for 1 block header for tail
    block_header_t* bh = create_block(PAGE_SIZE-3*BLOCK_HEADER_SIZE, ptr);
    bh = JMP_TO_NEXT_BH(bh);
    tail = bh+1;
    *tail = create_block_header(0, ALLOCATED);

    page_allocs += 1;
    return (block_header_t*) base_address;
}
    
static void init_heap() {
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* addr = (void*) mmap((void*) HEAP_START, PAGE_SIZE, prot, flags, -1, 0);
    head = create_free_list(addr);
}

static block_header_t* create_block(uint32_t requested_size, void* dest) {

    int revised_size = requested_size + 2*BLOCK_HEADER_SIZE;
    block_header_t* bh;
    block_header_t bh_front = create_block_header(revised_size, FREE);
    block_header_t bh_end = create_block_header(revised_size, FREE);

    bh = (block_header_t*) dest;
    *bh = bh_front;

    bh = JMP_TO_NEXT_BH(bh);
    printf("Block creation:\n\tbh start:%#x block size: %#x\n", (uintptr_t) dest, revised_size);
    *bh = bh_end;

    return (block_header_t*) dest;

}

// removes old tail block and connects blocks from list to newly created list
// pseudo_head: "head" of new list to join. needs to be contigous with tail
static void connect_new_free_block(block_header_t* pseudo_head) {
    block_header_t* bh = pseudo_head - 2;

    int prev_status = bh->status;
    void* ptr = bh;
    ptr -= GET_PAYLOAD_SIZE(bh) + BLOCK_HEADER_SIZE;
    bh = ptr;
    
    int new_size = bh->size + BLOCK_HEADER_SIZE;
    bh->size = new_size;

    ptr += bh->size - BLOCK_HEADER_SIZE;
    bh = ptr;

    *bh = create_block_header(new_size, prev_status);
}

static void allocate_block(block_header_t* bh, uint32_t requested_size) {
    SET_ALLOCATED(bh);
    bh->size = requested_size + BLOCK_HEADER_SIZE*2;
    // set new size 
    SET_ALLOCATED(JMP_TO_NEXT_BH(bh));
    void* ptr = bh;
}

static void free_block(block_header_t* bh) {
    SET_FREE(bh);
    memset((void*)bh+BLOCK_HEADER_SIZE, 0, GET_PAYLOAD_SIZE(bh));
    SET_FREE(JMP_TO_NEXT_BH(bh));
}

static void* split_block(block_header_t* free_bh, uint32_t requested_size) {
    uint32_t prev_free_space = free_bh->size;

    block_header_t* to_alloc_bh = create_block(requested_size, free_bh);
    allocate_block(to_alloc_bh, requested_size);
    
    // to_alloc_bh size will account for header size . use the remaining space 
    void* new_free_bh = ((void*) to_alloc_bh) + to_alloc_bh->size;
    int remaining_space = prev_free_space-to_alloc_bh->size;
    printf("remaining space: %u\n", remaining_space);
    create_block(remaining_space - BLOCK_HEADER_SIZE*2, new_free_bh);

    return to_alloc_bh;
}

static void* first_fit(uint32_t size){
    void* ptr = (void*) head;
    block_header_t* bh = (block_header_t*) head;

    int i = 0;
    while (bh->size != 0) {
        if (IS_VALID_BLOCK(bh, size)) {
            // take old free space of block you want to consume 
            // create new block with requested size and other new block with free space
            block_header_t* allocd_bh = split_block(bh, size); 
            printf("Allocating memory at address 0x%x\nWithin block %u\nblock size of 0x%x bytes\n------------\n",
            (uintptr_t) (((void*) allocd_bh) + BLOCK_HEADER_SIZE), i, bh->size);
            allocs += 1;
            return ((void*) allocd_bh) + BLOCK_HEADER_SIZE; 
        }

        ptr +=  bh->size;
        bh = (block_header_t*) ptr;
        i++;
    }

    // allocate additional memory
    void* addr = (void*) mmap(tail+1, PAGE_SIZE, MMAP_PROT, MMAP_FLAGS, -1, 0);
    bh = create_free_list(addr);
    connect_new_free_block(bh);
    coalesce(bh);
    return first_fit(size);
    
}

void* kmalloc(uint32_t size){
    if (head == NULL) {
        init_heap();
    }

    if (size == 0) {
        return NULL;    
    }
    return first_fit(size);
}

// merges two blocks. 
// params - bh: the 2nd front_bh of the two contingous blocks to be merged.
// returns new bh_start of coalesced blocks. 
static block_header_t* merge_from_below(block_header_t* bh) {
    block_header_t* new_bh_start;
    int new_block_size = bh->size + (bh-1)->size;
    // go up one bh
    bh -= 1;
    void* ptr = bh;
    // jump to start of new block and clear old block headers
    ptr -= BLOCK_HEADER_SIZE + GET_PAYLOAD_SIZE(bh);
    memset(bh, 0, BLOCK_HEADER_SIZE*2);
    // we're now at the front bh of newly coalesced block. we will return this address
    new_bh_start = (block_header_t*) ptr;
    bh = (block_header_t *)ptr;
    bh->size = new_block_size; 

    // jump to second block header of newly coalesced block
    ptr += new_block_size-BLOCK_HEADER_SIZE;
    bh = ptr;

    // set new meta
    bh->size = new_block_size; 

    return new_bh_start;

}

void coalesce(block_header_t* bh) {
    printf("Coalescing initiator bh at : 0x%x\n", bh);
    // if the current block that has been free has a prev or next block free, then merge
    if (bh - 1 > head && (bh-1)->status == FREE) {
        // now that we've this block with the one above, we need to use bh of newly coalesced block.
        bh = merge_from_below(bh);
        printf("Coalesced above block\n");
    } else {
        printf("Did not coalesce above allocated memory\n");
    }

    bh = JMP_TO_NEXT_BH(bh);
    // at this stage, bh will be at bh_end of freed payload. e.g.
    // | bh_start | payload | bh_end | bh_start | ...
    //                      ^  
    //                      bh
    if (bh + 1 >= tail || (bh+1)->status == ALLOCATED) {
        printf("Did not coalesce below allocated memory\n");
        return;
    }
    // point to bh_start of below block, so we can re-use merge_from_below
    bh += 1;
    merge_from_below(bh);
    printf("coalesced below block\n");
}


void free(void* ptr) {

    block_header_t* bh = (block_header_t*) (ptr - BLOCK_HEADER_SIZE);

    if (bh->size == 0) {
        return;
    }
    free_block(bh);
    allocs -= 1;

    if (allocs == 0) {
        munmap(HEAP_START, page_allocs*PAGE_SIZE);
        head = NULL;
        tail = NULL;
        page_allocs = 0;
        return;
    }

    coalesce(bh);
}

void resize_adjacent_blocks(
    block_header_t* bh1_start,
    block_header_t* bh1_end, 
    block_header_t* bh2_start, 
    block_header_t* bh2_end, 
    int expansion_size
) 
{
    void* new_bh1_end = (void*) bh1_end + expansion_size;
    void* bh1_end_vptr = (void*) bh1_end;
    uint8_t* new_bh2_start = bh1_end_vptr + BLOCK_HEADER_SIZE;

    bh1_end = memcpy(new_bh1_end, bh1_end_vptr, BLOCK_HEADER_SIZE*2); 

    bh2_start = bh1_end + 1;

    *bh2_start = create_block_header(bh2_end->size-expansion_size, bh2_end->status); 
    bh2_end = JMP_TO_NEXT_BH(bh2_start);
    bh2_end->size -= expansion_size;

    bh1_start->size += expansion_size;
    *bh1_end = create_block_header(bh1_start->size, ALLOCATED);

}

void* realloc(void* ptr, size_t size) {
    block_header_t* bh1_start = ptr-BLOCK_HEADER_SIZE;
    block_header_t* bh1_end = JMP_TO_NEXT_BH(bh1_start);

    block_header_t* bh2_start = bh1_end + 1;
    block_header_t* bh2_end = JMP_TO_NEXT_BH(bh2_start);
    int expansion_size = size - GET_PAYLOAD_SIZE(bh1_start);

    if (size <= 0) {
        free(ptr);
    } else if (expansion_size == 0 || ptr == NULL) {
        return ptr;
    } else if (expansion_size < 0 || bh2_start->status == FREE && bh2_start->size + bh1_start->size - BLOCK_HEADER_SIZE*4 >= size) {
        resize_adjacent_blocks(bh1_start, bh1_end, bh2_start, bh2_end, expansion_size);
        return ptr;
    } else {
        void* new_p = kmalloc(size);
        memcpy(new_p, ptr, GET_PAYLOAD_SIZE(bh1_start));
        free(ptr);
        return new_p;
    }
}

void list_status_logger(int from, int to) {
    if (head == NULL || tail == NULL) {
        return;
    }
    printf("<-----------------------------BLOCK------------------------------------------------>\n");
    block_header_t* bh = head;
    int n = 0;
    while (bh->size != 0 && ( n < to)) {
        printf("\nbh_front\n");
        printf("Block number: %i\naddress: %#x\nbh->size: %i\nbh->status: %i\n", n, (uintptr_t) bh, bh->size, bh->status);
        printf("\nbh_end\n");

        if (n%5 == 0){
            printf("head addr: %#x | tail addr %#x\n", (uintptr_t)head, (uintptr_t)tail);
        }

        bh = JMP_TO_NEXT_BH(bh);
        printf("Block number: %i\naddress: %#x\nbh->size: %i\nbh->status: %i\n", n, (uintptr_t) bh, bh->size, bh->status);
        printf("\n<-----------------------------BLOCK------------------------------------------------>\n");
        n += 1;
        bh += 1;
    }
}

