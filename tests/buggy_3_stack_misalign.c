// buggy_3_stack_misalign.c
// EXPECT: warning: stack pointer not restored; net RSP delta = 8 bytes
//
// Pushes a value but never pops it. When the asm block exits,
// %rsp is 8 bytes off, breaking the System V requirement that
// %rsp % 16 == 0 before any 'call'.
void stack_misalign() {
    asm volatile(
        "pushq %%rbx\n\t"  // RSP -= 8
        "movq $0, %%rbx\n\t"
        // Missing: popq %rbx   <- BUG
        ::: "rbx"
    );
}
