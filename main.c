#include <stdio.h>
#include "kmalloc.h"
#include <stdlib.h>

int main(void) {

    int i;
    for (i = 0; i < 4000; i++) {
        kmalloc(sizeof(char));
    }

    // char* s1 = (char*) kmalloc(500*sizeof(char));
    // char* s2 = (char*) kmalloc(500*sizeof(char));
    // char* s3 = (char*) kmalloc(500*sizeof(char));

    // int* p = malloc(1);

    // *p = 2;
    
    // free(s1);
    // free(s2);
    // free(s3);
    // char* s5 = (char*) kmalloc(200*sizeof(char));

    list_status_logger(0, 5000);


    return 0;
}