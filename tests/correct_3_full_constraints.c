// correct_3_full_constraints.c
// EXPECT: no diagnostics
//
// Modifies %rbx (callee-saved) by correctly saving and restoring it
// with push/pop and also declaring it in the clobber list as a
// belt-and-suspenders measure.
void safe_increment(int *x) {
    asm volatile(
        "incl %0"
        : "+m"(*x)
        :
        : "memory"
    );
}