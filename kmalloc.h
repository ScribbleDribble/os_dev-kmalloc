
#ifndef _KMALLOC_H
#define _KMALLOC_H


#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "../vmm.h"

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


void* realloc(void*, uint32_t size);

void* kmalloc(uint32_t size);

void free(void* ptr);

void list_status_logger(int from, int to);

#endif