#include <stdio.h>

void myOrder() {
    union {
        short val;
        char union_bytes[sizeof(short)];
    } test;
    test.val = 0x0102;
    if(test.union_bytes[0] == 1 && test.union_bytes[1] == 2)
        printf("Big endian\n");
    else if(test.union_bytes[0] == 2 && test.union_bytes[1] == 1)
        printf("Little endian\n");
    else
        printf("Unknown\n");
}

int main()
{
    myOrder();
    return 0;
}
