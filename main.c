#include <stdio.h>
#include "local_vmm.h"


int main(void) {

    init_heap();

    int x = 5;
    char* s1 = (char*) kmalloc(x*sizeof(char));
    // char* s2 = (char*) kmalloc(50*sizeof(char));

    char* s3 = (char*) kmalloc(x*sizeof(char));

    char* s4 = (char*)kmalloc(x*sizeof(char));
    
    free(s3);
    // free(s4);
    // s = "hey\0";
    // printf("%i", *get_num2());
    // int* b_ptr = (int*)kmalloc(sizeof(a));
    return 0;
}