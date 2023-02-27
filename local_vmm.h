#include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <sys/mman.h>


typedef struct block_header_t {
    uint32_t space : 3;
    uint32_t block_size : 29;
}block_header_t;

typedef struct block_t {
    block_header_t bh_front;
    block_header_t bh_end;
    void* payload_ptr;
}block_t;

#define DEFAULT_BLOCK_SIZE_BYTES 16
#define PAGE_SIZE 0x1000

// 8-bit aligned bh 
block_header_t root_bh;


static block_header_t create_block_header(uint16_t block_size, bool is_free) {
    block_header_t bh;
    bh.block_size = block_size;
    bh.space = is_free;
    return bh;
}

static block_t create_block(uint16_t block_size, bool is_free) {
    block_header_t start_bh = create_block_header(block_size, is_free);
    block_header_t end_bh = create_block_header(block_size, is_free);
    
    block_t block;
    block.bh_front = start_bh;
    block.bh_end = end_bh;

    printf("size of block: %luB\n", sizeof(block));

    return block;
}



void first_fit(){
    
}

void* kmalloc(uint32_t size){

}


void init_heap() {
    // get first page from kernel page table 

    // start addr of block of free memory
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* addr = (void*) mmap((void*) 0x1337, 0x1337, prot, flags, -1, 0);
    
    create_block(DEFAULT_BLOCK_SIZE_BYTES, true);
}

void create_free_list() {
    int i;
    int end = PAGE_SIZE/DEFAULT_BLOCK_SIZE_BYTES;
    for (i = 0; i < end; i++) {
        create_block(DEFAULT_BLOCK_SIZE_BYTES, true);
    }
}