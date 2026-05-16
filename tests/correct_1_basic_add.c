// correct_1_basic_add.c
// EXPECT: no diagnostics
//
// Uses output constraints and clobber declarations correctly.
// No register is modified outside the constraint system.
#include <stdio.h>

int add_asm(int a, int b) {
    int result;
    asm("addl %k1, %k0"
        : "=r"(result)       // output: result in any register
        : "r"(a), "0"(b)     // inputs
        :                    // no clobbers needed — only operand regs used
    );
    return result;
}
