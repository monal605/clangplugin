// buggy_1_missing_clobber.c
// EXPECT: warning: register '%eax' written but not in clobber list
//
// This test modifies %eax without declaring it in the clobber list.
// The compiler may keep a live value in %eax across the asm block,
// which gets silently overwritten.
#include <stdio.h>

void missing_clobber() {
    int result;
    asm("movl $42, %%eax"  // writes eax — not declared as clobber!
        : "=r"(result)
        :                  // no inputs
        );                 // no clobbers — BUG
    printf("%d\n", result);
}
