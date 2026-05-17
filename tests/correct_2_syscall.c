// correct_2_syscall.c
// EXPECT: no diagnostics
//
// A 'write' syscall that correctly declares all registers it touches
// (rax, rdi, rsi, rdx) in the clobber list.
void increment(int *x) {
    asm volatile(
        "incl %0"
        : "+m"(*x)
        :
        : "memory"
    );
}
