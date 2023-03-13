// // // #include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "kmalloc.h"
#include "../vmm.h"
#include "../../drivers/vga.h"

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

block_header_t* create_free_list(void*);

static block_header_t create_block_header(uint32_t size, bool is_allocated) {
    block_header_t bh;

    bh.size = size;
    bh.status = is_allocated;
    return bh;
}


static void init_heap() {
    void* addr = palloc(KERNEL_PD_INDEX, 1);
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
    *bh = bh_end;

    return (block_header_t*) dest;

}

block_header_t* create_free_list(void* base_address) {
    // returns the entry point to a new free 4kb block of memory 
    void* ptr = base_address;

    // one big free block that holds head and tail.
    // 2*block header will be added by create block. save space for 1 block header for tail
    block_header_t* bh = create_block(PAGE_SIZE-3*BLOCK_HEADER_SIZE, ptr);
    bh = JMP_TO_NEXT_BH(bh);
    tail = bh+1;
    *tail = create_block_header(0, ALLOCATED);

    return (block_header_t*) base_address;
}
    

// // removes old tail block and connects blocks from list to newly created list
// // pseudo_head: "head" of new list to join. needs to be contigous with tail
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
}

static void free_block(block_header_t* bh) {
    SET_FREE(bh);
    memory_set((void*)bh+BLOCK_HEADER_SIZE, 0, GET_PAYLOAD_SIZE(bh));
    SET_FREE(JMP_TO_NEXT_BH(bh));
}

static void* split_block(block_header_t* free_bh, uint32_t requested_size) {
    uint32_t prev_free_space = free_bh->size;

    block_header_t* to_alloc_bh = create_block(requested_size, free_bh);
    allocate_block(to_alloc_bh, requested_size);
    
    // to_alloc_bh size will account for header size . use the remaining space 
    void* new_free_bh = ((void*) to_alloc_bh) + to_alloc_bh->size;
    int remaining_space = prev_free_space-to_alloc_bh->size;
    create_block(remaining_space - BLOCK_HEADER_SIZE*2, new_free_bh);

    return to_alloc_bh;
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
    memory_set(bh, 0, BLOCK_HEADER_SIZE*2);
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


static void coalesce(block_header_t* bh) {
    // if the current block that has been free has a prev or next block free, then merge
    if (bh - 1 > head && (bh-1)->status == FREE) {
        // now that we've this block with the one above, we need to use bh of newly coalesced block.
        bh = merge_from_below(bh);
    } else {
    }

    bh = JMP_TO_NEXT_BH(bh);
    // at this stage, bh will be at bh_end of freed payload. e.g.
    // | bh_start | payload | bh_end | bh_start | ...
    //                      ^  
    //                      bh
    if (bh + 1 >= tail || (bh+1)->status == ALLOCATED) {
        return;
    }
    // point to bh_start of below block, so we can re-use merge_from_below
    bh += 1;
    merge_from_below(bh);
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
            allocs += 1;
            return ((void*) allocd_bh) + BLOCK_HEADER_SIZE; 
        }

        ptr +=  bh->size;
        bh = (block_header_t*) ptr;
        i++;
    }

    // allocate additional memory
    void* addr = palloc(KERNEL_PD_INDEX, 1);
    char buf[32];
    kputs("creating new 4kb block starting from below address:");
    // clear_screen();
    int_to_hex_str((uint32_t) addr, buf, 32);
    kputs(buf);
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

void free(void* ptr) {

    block_header_t* bh = (block_header_t*) (ptr - BLOCK_HEADER_SIZE);

    if (bh->size == 0) {
        return;
    }

    free_block(bh);
    allocs -= 1;

    coalesce(bh);
}

void list_status_logger(int from, int to) {
    if (head == -1 || tail == -1) {
        return;
    }

    block_header_t* bh = head;
    int n = 0;
    while (bh->size != 0 && ( n < to)) {

        if (n%5 == 0){
        }

        bh = JMP_TO_NEXT_BH(bh);

        n += 1;
        bh += 1;
    }
}

