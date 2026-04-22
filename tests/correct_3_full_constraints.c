// correct_3_full_constraints.c
// EXPECT: no diagnostics
//
// Modifies %rbx (callee-saved) by correctly saving and restoring it
// with push/pop and also declaring it in the clobber list as a
// belt-and-suspenders measure.
void safe_rbx_use() {
    asm volatile(
        "pushq %%rbx\n\t"      // save callee-saved register
        "movq  $42, %%rbx\n\t"
        "popq  %%rbx\n\t"      // restore — stack balanced
        ::: "rbx"              // clobber declared
    );
}
