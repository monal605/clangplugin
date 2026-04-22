// correct_2_syscall.c
// EXPECT: no diagnostics
//
// A 'write' syscall that correctly declares all registers it touches
// (rax, rdi, rsi, rdx) in the clobber list.
void write_syscall(const char* msg, long len) {
    asm volatile(
        "movq $1,  %%rax\n\t"  // syscall number: write
        "movq $1,  %%rdi\n\t"  // fd: stdout
        "movq %0,  %%rsi\n\t"  // buf
        "movq %1,  %%rdx\n\t"  // len
        "syscall\n\t"
        :
        : "r"(msg), "r"(len)
        : "rax", "rdi", "rsi", "rdx", "memory"  // all clobbers declared
    );
}
