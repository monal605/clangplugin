# Clang Inline Assembly Validator — Project Report

## 1. Project Overview

This project implements a **Clang compiler plugin** that performs static validation of x86-64 inline assembly statements in C/C++ source code. The plugin hooks into Clang's AST (Abstract Syntax Tree) pipeline, intercepts `GCCAsmStmt` nodes (GCC-style inline assembly), tokenizes the assembly string, and emits precise diagnostics for common inline assembly mistakes.

**Target ABI:** x86-64 System V AMD64
**Asm Syntax:** GCC-style extended asm (`asm("..." : outputs : inputs : clobbers)`)
**Clang Version:** 16+

---

## 2. Problem Statement

Inline assembly is a **compiler black box**. When a developer writes `asm("mov %rax, %rbx")`, the compiler trusts the programmer entirely — it does not parse the instruction string, does not verify register names, and does not check whether caller-saved registers are properly preserved.

This creates bugs that survive compilation and testing, only manifesting at runtime under specific conditions:

| Bug Category | Example | Consequence |
|---|---|---|
| Missing clobber declaration | Writing `%rax` without listing it as clobber | Caller's live value in `%rax` silently corrupted |
| Invalid register name | Using `%eax64` (typo) | Assembler error at best, undefined behavior at worst |
| Stack misalignment | `push` without matching `pop` | Violates 16-byte alignment; crashes on `call` |
| Callee-saved corruption | Modifying `%r13` without save/restore | ABI violation; caller's state destroyed |

---

## 3. Architecture

### 3.1 Plugin Pipeline

```
Source File (.c / .cpp)
        |
        v
   Clang Frontend
        |
        v
    AST Builder
        |
        v
  ASTFrontendAction  <-- Plugin Entry Point
        |
        v
  ASTConsumer::HandleTranslationUnit()
        |
        v
  RecursiveASTVisitor: GCCAsmStmt
        |
        +-- Extract asm string literal
        +-- Extract constraint annotations
        +-- Extract clobber list
        |
        v
  Tokenizer / Validator
        |
        +-- Pass 1: Register name validation
        +-- Pass 2: Clobber declaration check
        +-- Pass 3: Stack alignment analysis
        +-- Pass 4: Callee-saved register check
        |
        v
  DiagnosticsEngine (error / warning / note)
```

### 3.2 Key Clang APIs Used

| Component | Clang API |
|---|---|
| Plugin registration | `FrontendPluginRegistry::Add<>` |
| AST entry point | `PluginASTAction` -> `CreateASTConsumer()` |
| AST traversal | `RecursiveASTVisitor<T>` |
| Inline asm node | `GCCAsmStmt` |
| Assembly string | `GCCAsmStmt::getAsmString()` |
| Clobber list | `GCCAsmStmt::getClobber(i)` |
| Source location | `SourceManager`, `FullSourceLoc` |
| Diagnostics | `DiagnosticsEngine::Report()`, `DiagnosticBuilder` |

---

## 4. Project Structure

```
asm-validator/
+-- CMakeLists.txt                  # Build configuration
+-- plugin/
|   +-- AsmValidator.cpp            # Main plugin: AST visitor + 4 validation passes
|   +-- AsmTokenizer.h / .cpp       # Tokenizer for inline asm strings
|   +-- RegisterTable.h             # x86-64 register name lookup tables
|   +-- CallingConvention.h         # ABI register classification constants
+-- tests/
|   +-- buggy_1_missing_clobber.c   # Missing clobber declaration
|   +-- buggy_2_invalid_register.c  # Invalid register name
|   +-- buggy_3_stack_misalign.c    # Stack pointer not restored
|   +-- buggy_4_callee_saved.c      # Callee-saved register corrupted
|   +-- correct_1_basic_add.c       # Well-formed arithmetic
|   +-- correct_2_syscall.c         # Correct syscall with clobbers
|   +-- correct_3_full_constraints.c# Push/pop pair with clobber
+-- run_tests.sh                    # Automated test runner
+-- REPORT.md                       # This document
```

---

## 5. Implementation Details

### 5.1 Register Table (`RegisterTable.h`)

All valid x86-64 register names are stored in an `std::unordered_set<std::string>` for O(1) lookup. Three sets are defined:

- **VALID_REGISTERS** — Every architectural register name (64-bit GPRs, 32/16/8-bit aliases, segment registers, XMM0-15, YMM0-7)
- **CALLER_SAVED** — Volatile registers per System V ABI: `rax, rcx, rdx, rsi, rdi, r8-r11, xmm0-7`
- **CALLEE_SAVED** — Non-volatile registers: `rbx, rbp, r12-r15`

### 5.2 Tokenizer (`AsmTokenizer.cpp`)

The tokenizer processes an inline asm string into `AsmInstruction` structs:

1. **Split** the asm string on `\n` and `;` to get individual statements
2. **Skip** labels (ending with `:`) and assembler directives (starting with `.`)
3. **Extract** the mnemonic (first token, lowercased)
4. **Classify** each operand:
   - `%reg` -> Register (AT&T syntax, strip `%`)
   - `$imm` or digit -> Immediate
   - `(...)` or `[...]` -> Memory
   - Known register name -> Register (Intel syntax fallback)
   - Otherwise -> Unknown

### 5.3 Validation Passes (`AsmValidator.cpp`)

**Pass 1 — Register Name Validation:**
Every operand classified as `Register` is checked against `VALID_REGISTERS`. Unknown names emit an **error**-level diagnostic.

**Pass 2 — Clobber Declaration Check:**
For write-class mnemonics (`mov`, `add`, `sub`, `xor`, `lea`, `pop`, etc.), the destination operand (last operand in AT&T syntax) is checked. If it is a caller-saved register not present in the declared clobber list, a **warning** is emitted.

**Pass 3 — Stack Alignment Analysis:**
The pass tracks a `stackDelta` counter:
- `push`/`pushq` -> +8
- `pop`/`popq` -> -8
- `sub $imm, %rsp` -> +imm

If `stackDelta != 0` at the end of the asm block, a **warning** is emitted indicating the stack is unbalanced.

**Pass 4 — Callee-Saved Register Check:**
Tracks which callee-saved registers are `push`ed (saved) and `pop`ped (restored). If a callee-saved register is written by a write-class mnemonic without being previously saved AND without being declared in the clobber list, an **error** is emitted. Additionally, any register that is pushed but never popped triggers an **error**.

---

## 6. Test Suite

### 6.1 Buggy Test Cases (Expected Diagnostics)

| File | Bug Type | Expected Diagnostic |
|---|---|---|
| `buggy_1_missing_clobber.c` | Writes `%eax` without clobber | warning: register written but not in clobber list |
| `buggy_2_invalid_register.c` | Uses `%eax64` (invalid) | error: unknown x86-64 register |
| `buggy_3_stack_misalign.c` | `push` without `pop` | warning: stack pointer not restored |
| `buggy_4_callee_saved.c` | Writes `%r13` without save | error: callee-saved register modified without push/restore |

### 6.2 Correct Test Cases (No Diagnostics Expected)

| File | Pattern | Why It's Correct |
|---|---|---|
| `correct_1_basic_add.c` | Constrained operands only | All register usage is via output/input constraints |
| `correct_2_syscall.c` | All clobbers declared | `rax`, `rdi`, `rsi`, `rdx`, `memory` all listed |
| `correct_3_full_constraints.c` | Push/pop + clobber | `%rbx` saved, restored, and declared as clobber |

### 6.3 Sample Diagnostic Output

```
buggy_1_missing_clobber.c:6:5: warning: inline asm: register '%eax' is written
    but not listed in clobber list
buggy_2_invalid_register.c:4:5: error: inline asm: unknown x86-64 register '%eax64'
buggy_3_stack_misalign.c:4:5: warning: inline asm: stack pointer is not restored;
    net RSP delta = 8 bytes — violates 16-byte alignment contract
buggy_4_callee_saved.c:5:5: error: inline asm: callee-saved register '%r13'
    is modified without being pushed/restored — ABI violation
```

---

## 7. Build & Run Instructions

### 7.1 Prerequisites

- Clang/LLVM 16+ development libraries
- CMake 3.16+
- A C++17-capable compiler

### 7.2 Build the Plugin

```bash
cd asm-validator/
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DLLVM_DIR=$(llvm-config --cmakedir) \
      -DClang_DIR=$(clang --print-resource-dir)/../../lib/cmake/clang
cmake --build build --target AsmValidator
```

This produces `build/libAsmValidator.so`.

### 7.3 Run on a Single File

```bash
clang -fplugin=./build/libAsmValidator.so \
      -fsyntax-only tests/buggy_1_missing_clobber.c
```

### 7.4 Run the Full Test Suite

```bash
chmod +x run_tests.sh
./run_tests.sh
```

---

## 8. Diagnostic Severity Levels

| Severity | When Used | Example |
|---|---|---|
| **error** | Invalid register name; callee-saved corruption | `unknown x86-64 register '%eax64'` |
| **warning** | Missing clobber; stack imbalance | `register '%rax' written but not in clobber list` |
| **note** | (future) Suggested fix-it hints | — |

All diagnostics are emitted through Clang's `DiagnosticsEngine`, integrating seamlessly with standard compiler output and respecting flags like `-Werror`, `-w`, and IDE error parsers.

---

## 9. Limitations — What Static Analysis Cannot Detect

### 9.1 Data-Flow-Dependent Register Values
The plugin operates on the assembly string as text. Conditional clobbers (e.g., a `mov` behind a `je`) cannot be resolved statically without symbolic execution.

### 9.2 Indirect Memory Operands and Aliasing
Instructions like `movq (%rax), %rbx` write to memory at an address in `%rax`. Full alias analysis with heap modeling would be needed to determine which memory locations are affected.

### 9.3 SIMD and Vector Register Partial Writes
AVX-to-SSE transitions may require `vzeroupper`. The plugin cannot determine surrounding vector register context.

### 9.4 Control Flow Within asm Blocks
Internal labels and conditional jumps (e.g., `jmp .exit` skipping a `push`) cause false positives because the tokenizer does not model intra-block control flow.

### 9.5 Self-Modifying Code
If the asm string is constructed at runtime, the plugin receives an empty or partial string — nothing to validate.

### 9.6 CPU Feature-Flag Dependencies
Instructions like `rdrand` or `vpclmulqdq` are only valid with specific CPUID bits. The plugin has no access to the target CPU profile at parse time.

### 9.7 Compiler Prologue/Epilogue Interaction
The plugin analyzes the asm block in isolation and cannot see compiler-generated frame setup/teardown code surrounding it.

---

## 10. Extension Opportunities

| Enhancement | Description |
|---|---|
| Intel-syntax support | Detect `INTEL_SYNTAX` prefix; parallel tokenizer path |
| Mnemonic validity | Extend with valid mnemonic set; flag unknown opcodes |
| Fix-it hints | Use `Diags.Report(...).AddFixItHint(...)` for suggested clobbers |
| clang-tidy integration | Repackage as a clang-tidy check for IDE support |
| Extended SIMD coverage | Add `ymm0-15`, `zmm0-31`, and `k` mask registers |

---

## 11. References

1. GCC Extended Asm — https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html
2. System V AMD64 ABI — https://gitlab.com/x86-psABIs/x86-64-ABI
3. Clang Plugin Tutorial — https://clang.llvm.org/docs/ClangPlugins.html
4. Clang GCCAsmStmt API — https://clang.llvm.org/doxygen/classclang_1_1GCCAsmStmt.html
5. LLVM DiagnosticsEngine — https://clang.llvm.org/doxygen/classclang_1_1DiagnosticsEngine.html
