// correct_1_basic_add.c
// EXPECT: no diagnostics
//
// Uses output constraints and clobber declarations correctly.
// No register is modified outside the constraint system.
#include <stdio.h>

int main() {
    int x = 5;

    asm("incl %0"
        : "+r"(x)
        :
        : "cc");

    return 0;
}