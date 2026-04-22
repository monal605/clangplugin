// buggy_2_invalid_register.c
// EXPECT: error: unknown x86-64 register '%eax64'
//
// Uses a non-existent register name '%eax64', a common mistype
// that GCC-style asm does not validate at compile time.
void invalid_register() {
    asm volatile(
        "movq $1, %%eax64" // '%%eax64' does not exist
        :::
    );
}
