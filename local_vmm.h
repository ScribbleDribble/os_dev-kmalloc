// #include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>

/*

    Block:

    -----------------------------
    front block header (4 bytes)

    -----------------------------

            payload 

    -----------------------------
    
    padding (to make sure Block is 8-bit aligned in memory)

    ------------------------------

     back block header (4 bytes)
    
    ------------------------------
*/

typedef struct block_header_t {
    uint32_t status : 3;
    uint32_t size : 29;
}block_header_t;

typedef struct block_t {
    block_header_t bh_front;
    block_header_t bh_back;
    void* payload_ptr;
}block_t;

#define BLOCK_HEADER_SIZE sizeof(block_header_t)
#define DEFAULT_BLOCK_SIZE_BYTES 16
#define DEFAULT_PAYLOAD_SIZE_BYTES 8
#define PAGE_SIZE 0x1000

#define IS_ALLOCATED(block_header) (block_header->status & 1) 
#define IS_FITTING(block_header, requested_size) (requested_size > block_header->size - 2 * sizeof(block_header))
#define IS_VALID_BLOCK(block_header, requested_size) (IS_ALLOCATED(block_header) && IS_FITTING(block_header, requested_size))

block_header_t* head;

static block_header_t create_block_header(uint16_t size, bool is_allocated) {
    block_header_t bh;
    bh.size = size;
    bh.status = is_allocated;
    return bh;
}

void* first_fit(uint32_t size){
    void* ptr = (void*) head;
    block_header_t* bh_ptr = (block_header_t*) head;
    while (bh_ptr->size != 0) {
        if (IS_VALID_BLOCK(bh_ptr, size)) {
            printf("Allocating memory at address 0x%x\n",(uint32_t) ((void*) ptr) + sizeof(block_header_t));
            return ((void*) ptr) + sizeof(block_header_t); 
        }
        ptr +=  bh_ptr->size;
        bh_ptr = (block_header_t*) ptr;
    } 
}

void* kmalloc(uint32_t size){
    return first_fit(size);
}



void create_free_list(void* base_address) {
    int i;
    int end = PAGE_SIZE/DEFAULT_BLOCK_SIZE_BYTES;
    
    head = (block_header_t*) base_address;
    void* ptr = base_address;
    printf("%x\n", ((uint32_t) ptr));

    for (i = 0; i < end-1; i++) {
        block_header_t bh_front = create_block_header(DEFAULT_BLOCK_SIZE_BYTES, true);
        *(block_header_t*)ptr = bh_front;
        // advance to where the back block header should be
        ptr += DEFAULT_PAYLOAD_SIZE_BYTES + sizeof(block_header_t);
        // printf("%u\n", (uint32_t) ptr);
        block_header_t bh_back = create_block_header(DEFAULT_BLOCK_SIZE_BYTES, true);
        *(block_header_t*)ptr = bh_back;

        // advance to where the next front bh should be 
        ptr += sizeof(block_header_t);
        
    }
    block_header_t list_tail_bh = create_block_header(0, true);
    *(block_header_t*)ptr = list_tail_bh;

}
    

void init_heap() {
    // get first page from kernel page table 

    // start addr of block of free memory
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* addr = (void*) mmap((void*) 0x50000, 0x1000, prot, flags, -1, 0);
    printf("%x heap start address\n", (uint32_t)addr);
    create_free_list(addr);
}
