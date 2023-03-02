#include "local_vmm.h"
#include <stdio.h>



int main(void) {

    init_heap();

    int x = 5;
    char* s = (char*) kmalloc(x*sizeof(char));

    // s = "hey\0";
    // printf("%i", *get_num2());
    // int* b_ptr = (int*)kmalloc(sizeof(a));
    return 0;
}