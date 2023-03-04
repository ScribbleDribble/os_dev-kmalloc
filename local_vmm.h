// #include <stdio.h> 
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

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

#define IS_ALLOCATED(block_header) (block_header->status & ALLOCATED) 
#define IS_FITTING(block_header, requested_size) (requested_size > block_header->size - 2 * sizeof(block_header))
#define IS_VALID_BLOCK(block_header, requested_size) (!IS_ALLOCATED(block_header) && IS_FITTING(block_header, requested_size))

#define JMP_TO_NEXT_BH(bh) ( (block_header_t*) ( (void*) bh + GET_PAYLOAD_SIZE(bh) + BLOCK_HEADER_SIZE) )

#define GET_PAYLOAD_SIZE(block_header) (block_header->size - BLOCK_HEADER_SIZE * 2)
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


void allocate_block(block_header_t* bh) {
    SET_ALLOCATED(bh);
    SET_ALLOCATED(JMP_TO_NEXT_BH(bh));
}

void free_block(block_header_t* bh) {
    SET_FREE(bh);
    printf("clearing %x\n", ((void*)bh+BLOCK_HEADER_SIZE));
    memset((void*)bh+BLOCK_HEADER_SIZE, 0, GET_PAYLOAD_SIZE(bh));
    SET_FREE(JMP_TO_NEXT_BH(bh));
}

void* first_fit(uint32_t size){
    void* ptr = (void*) head;
    block_header_t* bh_ptr = (block_header_t*) head;
    int i = 0;
    while (bh_ptr->size != 0) {
        if (IS_VALID_BLOCK(bh_ptr, size)) {
            // SET_ALLOCATED(bh_ptr);
            allocate_block(bh_ptr);
            printf("Allocating memory at address 0x%x\nWithin block %u\nblock size of 0x%x bytes\n------------\n",
            (uint32_t) ((void*) ptr) + BLOCK_HEADER_SIZE, i, bh_ptr->size);
            allocs += 1;
            return ((void*) ptr) + BLOCK_HEADER_SIZE; 
        }

        ptr +=  bh_ptr->size;
        bh_ptr = (block_header_t*) ptr;
        i++;
    }
    printf("Unable to allocate memory of size %u\n", size); 
}

// check address inputs for memset. 

void* kmalloc(uint32_t size){
    return first_fit(size);
}

void coalesce(block_header_t* bh_ptr) {

    int new_block_size = 0;

    // if the current block that has been free has a prev or next block free, then merge
    
    //
    // bounds checking
    if (bh_ptr - 1 > head && (bh_ptr-1)->status == FREE) {
        new_block_size = bh_ptr->size + (bh_ptr-1)->size;

        bh_ptr -= 1;
        void* ptr = bh_ptr;

        
        // jump to start of new block and clear old block headers
        ptr -= BLOCK_HEADER_SIZE + GET_PAYLOAD_SIZE(bh_ptr);
        memset(bh_ptr, 0, BLOCK_HEADER_SIZE*2);
        bh_ptr = (block_header_t *)ptr;
        *bh_ptr = create_block_header(new_block_size, FREE);
        
        ptr += new_block_size-BLOCK_HEADER_SIZE;
        bh_ptr = ptr;

        // set size of new block header end
        bh_ptr->size = new_block_size;    

        printf("coalesced above block\n");

    }

    // | bh_start | payload | bh_end | bh_start | ...
//                          ^  
//                         here

    printf("cur %x | tail %x", bh_ptr, tail);
    if (bh_ptr + 1 >= tail || (bh_ptr+1)->status != FREE) {
        printf("no\n");
        return;
    }

    // move ptr into similar position as above code.
    bh_ptr += 1;

    new_block_size = bh_ptr->size + (bh_ptr-1)->size;

    bh_ptr -= 1;
    void* ptr = bh_ptr;

    // jump to start of new block and clear old block headers
    ptr -= BLOCK_HEADER_SIZE + GET_PAYLOAD_SIZE(bh_ptr);
    memset(bh_ptr, 0, BLOCK_HEADER_SIZE*2);

    // we're now at bh_start of newly coalesced block
    bh_ptr = (block_header_t *)ptr;
    bh_ptr->size = new_block_size; 
    bh_ptr->status = FREE;

    ptr += new_block_size-BLOCK_HEADER_SIZE;
    bh_ptr = ptr;

    // set size of new block header end
    bh_ptr->size = new_block_size; 

    printf("coalesced below block\n");
    // check if prev block free
    // if free, 0 out the joining block headers, and then adjust the new bh front and end with the new infomation

}


void free(void* ptr) {

    block_header_t* bh = (block_header_t*) (ptr - BLOCK_HEADER_SIZE);
    printf("freeing pointer allocated at 0x%x\n", (uint32_t) ptr);
    free_block(bh);

    allocs -= 1;

    if (allocs == 0) {
        printf("freeing heap completely.\n");
        munmap((void*)head, 0x1000);
    }

    coalesce(bh);
}



void create_free_list(void* base_address) {
    int i;
    int end = PAGE_SIZE/DEFAULT_BLOCK_SIZE_BYTES;
    
    head = (block_header_t*) base_address;
    void* ptr = base_address;
    for (i = 0; i < end-1; i++) {
        block_header_t bh_front = create_block_header(DEFAULT_BLOCK_SIZE_BYTES, FREE);
        *(block_header_t*)ptr = bh_front;
        // advance to where the back block header should be
        ptr += DEFAULT_PAYLOAD_SIZE_BYTES + sizeof(block_header_t);

        block_header_t bh_back = create_block_header(DEFAULT_BLOCK_SIZE_BYTES, FREE);
        *(block_header_t*)ptr = bh_back;

        // advance to where the next front bh should be 
        ptr += sizeof(block_header_t);
        
    }
    tail = ptr;
    *tail = create_block_header(0, true);

}
    

void free_list_status_printer(int from, int to) {
}

void init_heap() {

    // start addr of block of free memory
    int prot = PROT_READ | PROT_WRITE;
    int flags = MAP_PRIVATE | MAP_ANONYMOUS;
    void* addr = (void*) mmap((void*) HEAP_START, PAGE_SIZE, prot, flags, -1, 0);
    printf("%x heap start address\n", (uint32_t)addr);
    create_free_list(addr);
}
