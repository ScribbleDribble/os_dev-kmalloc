#include <stdio.h>
#include "kmalloc.h"
#include <stdlib.h>

int main(void) {

    char* p = kmalloc(6);
    
    

    // memcpy(p, "ss", strlen("ss"));

    // printf(" %s",p);

    // list_status_logger(0, 20);


    // memcpy(new, old, BLOCK_HEADER_SIZE*2);

    // memcpy(d, "ss", strlen("ss"));

    // list_status_logger(0, 20);
    // list_status_logger(0, 20);
    // char* s = kmalloc(5);

    // char* x =  kmalloc(4);
    // // list_status_logger(0, 20);

    // free(s);

    // list_status_logger(0, 20);

    int i;
    for (i = 0; i < 4000; i++) {
        p = realloc(p, i);

    }
    
    // kmalloc(1);

    list_status_logger(0, 20);

    // *p = 2;
    
    // free(s1);
    // free(s2);
    // free(s3);
    // char* s5 = (char*) kmalloc(200*sizeof(char));



    return 0;
}