// buggy_4_callee_saved.c
// EXPECT: error: callee-saved register '%r13' modified without push/restore
//
// Modifies %r13 (a callee-saved register) without saving it first.
// The caller may rely on %r13 being preserved across this function call.
void callee_saved_violation() {
    long x = 100;
    asm volatile(
        "movq $999, %%r13" // r13 is callee-saved — must push/pop it
        : "+r"(x)
        ::
    );
}
