// #include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include <stdlib.h>

/*

    Block:

    ----------------------------- 0x50000
    front block header (4 bytes)                    

    ----------------------------- 0x50004

            payload 

    ----------------------------- 0x5000C
    
    optional padding (to make sure Block is 8-bit aligned in memory)

    ------------------------------0x5000C

     back block header (4 bytes)
    
    ------------------------------
*/

typedef struct block_header_t {
    uint32_t status : 3;
    uint32_t size : 29;
}block_header_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define DEFAULT_BLOCK_SIZE_BYTES 16
#define DEFAULT_PAYLOAD_SIZE_BYTES 8
#define PAGE_SIZE 0x1000
#define HEAP_START 0x50000

#define ALLOCATED 1
#define FREE 0 

#define GET_PAYLOAD_SIZE(block_header) (block_header->size - BLOCK_HEADER_SIZE * 2)
#define IS_ALLOCATED(block_header) (block_header->status & ALLOCATED) 
#define IS_FITTING(block_header, requested_size) (requested_size < GET_PAYLOAD_SIZE(block_header))
#define IS_VALID_BLOCK(block_header, requested_size) (!IS_ALLOCATED(block_header) && IS_FITTING(block_header, requested_size))

#define JMP_TO_NEXT_BH(bh) ( (block_header_t*) ( (void*) bh + GET_PAYLOAD_SIZE(bh) + BLOCK_HEADER_SIZE) )

#define SET_ALLOCATED(block_header) (block_header->status = ALLOCATED)
#define SET_FREE(block_header) (block_header->status = FREE)

block_header_t* head;
block_header_t* tail;

static uint32_t allocs = 0;

static block_header_t create_block_header(uint16_t size, bool is_allocated) {
    block_header_t bh;
    bh.size = size;
    bh.status = is_allocated;
    return bh;
}

void* create_block(uint32_t size, uint32_t* dest) {
    if ( ((uint32_t)dest * 8 % 8) != 0) {
        printf("Error in creating block: requested address to place bh at %i is not 8-bit aligned", (uint32_t) dest);
        exit(0);
    }

    block_header_t* bh;
    block_header_t bh_front = create_block_header(size, FREE);
    block_header_t bh_end = create_block_header(size, FREE);

    bh = (block_header_t*) dest;
    *bh = bh_front;
    
    bh = JMP_TO_NEXT_BH(bh);
    *bh = bh_end;

    // set new tail 
    bh += 1;
    tail = bh;
    *tail = create_block_header(0, true);

    return dest;

}

void allocate_block(block_header_t* bh) {
    SET_ALLOCATED(bh);
    SET_ALLOCATED(JMP_TO_NEXT_BH(bh));
    void* ptr = bh;
}

void free_block(block_header_t* bh) {
    SET_FREE(bh);
    memset((void*)bh+BLOCK_HEADER_SIZE, 0, GET_PAYLOAD_SIZE(bh));
    SET_FREE(JMP_TO_NEXT_BH(bh));
}

void* first_fit(uint32_t size){
    void* ptr = (void*) head;
    block_header_t* bh = (block_header_t*) head;
    int i = 0;
    while (bh->size != 0) {
        if (IS_VALID_BLOCK(bh, size)) {
            allocate_block(bh);
            printf("Allocating memory at address 0x%x\nWithin block %u\nblock size of 0x%x bytes\n------------\n",
            (uint32_t) ((void*) ptr) + BLOCK_HEADER_SIZE, i, bh->size);
            allocs += 1;
            return ((void*) ptr) + BLOCK_HEADER_SIZE; 
        }

        ptr +=  bh->size;
        bh = (block_header_t*) ptr;
        i++;
    }
    printf("Unable to allocate memory of size %u\n", size); 
}

void* kmalloc(uint32_t size){
    return first_fit(size);
}

// merges two blocks. 
// params - bh: the 2nd front_bh of the two contingous blocks to be merged.
// returns new bh_start of coalesced blocks. 
block_header_t* merge_from_below(block_header_t* bh) {
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
    bh->status = FREE;

    // jump to second block header of newly coalesced block
    ptr += new_block_size-BLOCK_HEADER_SIZE;
    bh = ptr;

    // set new meta
    bh->size = new_block_size; 
    bh->status = FREE;

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
        printf("Not a valid bh\n");
    }

    printf("freeing pointer allocated at 0x%x\n", (uint32_t) ptr);
    free_block(bh);
    allocs -= 1;

    coalesce(bh);
}


// create head 

// create tail

void create_free_list(void* base_address) {
    int i;
    int end = PAGE_SIZE/DEFAULT_BLOCK_SIZE_BYTES;
    
    head = (block_header_t*) base_address;
    void* ptr = base_address;


    for (i = 0; i < end-1; i++) {
        block_header_t* bh = create_block(DEFAULT_BLOCK_SIZE_BYTES, ptr);
        ptr = JMP_TO_NEXT_BH(bh);
        // advance to where the next front bh should be 
        ptr += sizeof(block_header_t);
    }
    

}
    

void list_status_logger(int from, int to) {

    printf("<-----------------------------BLOCK------------------------------------------------>\n");
    block_header_t* bh = head;
    int n = 0;
    while (bh->size != 0 && ( n < to )) {
        printf("\nbh_front\n");
        printf("Block number: %i\naddress: 0x%x\nbh->size: %i\nbh->status: %i\n", n, bh, bh->size, bh->status);
        printf("\nbh_end\n");
        bh = JMP_TO_NEXT_BH(bh);
        printf("Block number: %i\naddress: 0x%x\nbh->size: %i\nbh->status: %i\n", n, bh, bh->size, bh->status);
        printf("\n<-----------------------------BLOCK------------------------------------------------>\n");
        n += 1;
        bh += 1;
    }
}

void init_heap() {
    // start addr of block of free memory
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* addr = (void*) mmap((void*) HEAP_START, PAGE_SIZE, prot, flags, -1, 0);
    printf("%x heap start address\n", (uint32_t)addr);
    create_free_list(addr);
}
