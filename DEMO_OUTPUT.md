# Demo Output — Inline Assembly Validator for x86-64

This document captures actual output from running the validator, demonstrating both successful error detection and correct handling of valid code.

## Demo 1: Tool Usage / Help Screen

```
> .\asm_validator.exe

=========================================
  Clang Inline Assembly Validator v1.0
  x86-64 / System V AMD64 ABI
=========================================

Usage: asm_validator.exe <file.c> [file2.c ...]

Validates inline assembly in C/C++ source files.
Checks for:
  [1] Invalid register names
  [2] Missing clobber declarations
  [3] Stack pointer misalignment
  [4] Callee-saved register corruption
```

## Demo 2: Buggy Test Cases (Failure Detection)

Running the validator on all 4 buggy test files. Each file contains a specific inline assembly bug that the tool detects:

```
> .\asm_validator.exe tests/buggy_1_missing_clobber.c tests/buggy_2_invalid_register.c tests/buggy_3_stack_misalign.c tests/buggy_4_callee_saved.c

tests/buggy_1_missing_clobber.c:11: warning: inline asm: register '%eax' is written but not listed in clobber list
tests/buggy_2_invalid_register.c:7: error: inline asm: unknown x86-64 register '%eax64'
tests/buggy_3_stack_misalign.c:8: warning: inline asm: stack pointer is not restored; net RSP delta = 8 bytes -- violates 16-byte alignment contract
tests/buggy_3_stack_misalign.c:8: error: inline asm: callee-saved register '%rbx' is pushed but never popped -- stack will be misaligned
tests/buggy_4_callee_saved.c:8: error: inline asm: callee-saved register '%r13' is modified without being pushed/restored -- ABI violation

=========================================
  Validation Summary
=========================================
  Files scanned:    4
  Asm blocks found: 4
  Errors:           3
  Warnings:         2
=========================================
```

### What Each Diagnostic Means

| File | Line | Severity | Explanation |
|------|------|----------|-------------|
| `buggy_1_missing_clobber.c` | 11 | Warning | `movl $42, %eax` writes to `%eax` but it's not in the clobber list — the compiler might have a live value there |
| `buggy_2_invalid_register.c` | 7 | Error | `%eax64` is not a valid x86-64 register name — likely a typo |
| `buggy_3_stack_misalign.c` | 8 | Warning | `pushq %rbx` without a matching `popq` leaves RSP off by 8 bytes |
| `buggy_3_stack_misalign.c` | 8 | Error | `%rbx` (callee-saved) is pushed but never popped |
| `buggy_4_callee_saved.c` | 8 | Error | `%r13` (callee-saved) is modified with `movq` without being saved first |

## Demo 3: Correct Test Cases (No False Positives)

Running the validator on all 3 correct test files. The tool produces **zero diagnostics**, confirming no false positives:

```
> .\asm_validator.exe tests/correct_1_basic_add.c tests/correct_2_syscall.c tests/correct_3_full_constraints.c

=========================================
  Validation Summary
=========================================
  Files scanned:    3
  Asm blocks found: 4
  Errors:           0
  Warnings:         0
=========================================
```

### Why Each File Is Correct

| File | Pattern | Validator Behaviour |
|------|---------|---------------------|
| `correct_1_basic_add.c` | Uses `"=r"` / `"r"` constraints only | No explicit register writes outside constraints — nothing to flag |
| `correct_2_syscall.c` | Writes `%%rax, %%rdi, %%rsi, %%rdx` | All are declared in clobber list: `"rax", "rdi", "rsi", "rdx", "memory"` |
| `correct_3_full_constraints.c` | Modifies `%%rbx` (callee-saved) | Properly saved (`pushq`), restored (`popq`), and declared as clobber (`"rbx"`) |

## Demo 4: Individual File Analysis

### Buggy File #1: Missing Clobber

```c
// buggy_1_missing_clobber.c
void missing_clobber() {
    int result;
    asm("movl $42, %eax"      // writes eax — not declared as clobber!
        : "=r"(result)
        :                      // no inputs
        );                     // no clobbers — BUG
    printf("%d\n", result);
}
```

```
> .\asm_validator.exe tests/buggy_1_missing_clobber.c

tests/buggy_1_missing_clobber.c:11: warning: inline asm: register '%eax' is written but not listed in clobber list

=========================================
  Validation Summary
=========================================
  Files scanned:    1
  Asm blocks found: 1
  Errors:           0
  Warnings:         1
=========================================
```

### Buggy File #2: Invalid Register

```c
// buggy_2_invalid_register.c
void invalid_register() {
    asm volatile(
        "movq $1, %eax64"     // '%eax64' does not exist
        :::
    );
}
```

```
> .\asm_validator.exe tests/buggy_2_invalid_register.c

tests/buggy_2_invalid_register.c:7: error: inline asm: unknown x86-64 register '%eax64'

=========================================
  Validation Summary
=========================================
  Files scanned:    1
  Asm blocks found: 1
  Errors:           1
  Warnings:         0
=========================================
```

### Buggy File #3: Stack Misalignment

```c
// buggy_3_stack_misalign.c
void stack_misalign() {
    asm volatile(
        "pushq %rbx\n\t"      // RSP -= 8
        "movq $0, %rbx\n\t"
        // Missing: popq %rbx   <- BUG
        ::: "rbx"
    );
}
```

```
> .\asm_validator.exe tests/buggy_3_stack_misalign.c

tests/buggy_3_stack_misalign.c:8: warning: inline asm: stack pointer is not restored; net RSP delta = 8 bytes -- violates 16-byte alignment contract
tests/buggy_3_stack_misalign.c:8: error: inline asm: callee-saved register '%rbx' is pushed but never popped -- stack will be misaligned

=========================================
  Validation Summary
=========================================
  Files scanned:    1
  Asm blocks found: 1
  Errors:           1
  Warnings:         1
=========================================
```

### Buggy File #4: Callee-Saved Violation

```c
// buggy_4_callee_saved.c
void callee_saved_violation() {
    long x = 100;
    asm volatile(
        "movq $999, %r13"     // r13 is callee-saved — must push/pop
        : "+r"(x)
        ::
    );
}
```

```
> .\asm_validator.exe tests/buggy_4_callee_saved.c

tests/buggy_4_callee_saved.c:8: error: inline asm: callee-saved register '%r13' is modified without being pushed/restored -- ABI violation

=========================================
  Validation Summary
=========================================
  Files scanned:    1
  Asm blocks found: 1
  Errors:           1
  Warnings:         0
=========================================
```
