#include <stdio.h>
#include "local_vmm.h"


int main(void) {

    init_heap();

    int x = 5;
    char* s1 = (char*) kmalloc(x*sizeof(char));
    // char* s2 = (char*) kmalloc(50*sizeof(char));

    char* s2 = (char*) kmalloc(x*sizeof(char));

    char* s3 = (char*) kmalloc(x*sizeof(char));
    
    free(s1);
    free(s2);
    // free(s3);

    list_status_logger(0, 4);


    return 0;
}