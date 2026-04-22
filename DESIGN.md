# Design Document — Inline Assembly Validator for x86-64

## 1. Problem Definition

Inline assembly in C/C++ is a compiler black box. The compiler trusts the programmer's assembly string, constraint annotations, and clobber declarations without verification. This leads to subtle bugs that survive compilation and may only manifest at runtime under specific register-pressure or optimization scenarios.

**Goal:** Build a lightweight static validator that catches the most common classes of inline assembly mistakes at compile time, without requiring execution.

## 2. Design Approach

### 2.1 Chosen Architecture: Clang AST Plugin + Standalone Fallback

The project uses a **two-tier architecture**:

1. **Clang Plugin** (`plugin/`) — Hooks into Clang's AST pipeline via `PluginASTAction`, visits `GCCAsmStmt` nodes, and emits diagnostics through Clang's `DiagnosticsEngine`. This integrates seamlessly with the compiler and IDE error parsers.

2. **Standalone Binary** (`standalone/`) — A self-contained C++ program that parses `.c` files as text, extracts `asm(...)` blocks using parenthesis matching and string literal parsing, and runs the same four validation passes. Requires no LLVM/Clang libraries.

### 2.2 Why Two Tiers?

| Concern | Plugin | Standalone |
|---------|--------|------------|
| Accuracy | High — uses real AST, source locations | Moderate — text-based extraction |
| Dependencies | LLVM/Clang dev libs | Only a C++17 compiler |
| Portability | Linux/macOS (`.so` plugin) | Linux, macOS, Windows |
| IDE integration | Native (warnings/errors in editor) | File:line text output |
| Build complexity | CMake + LLVM paths | Single `g++` command |

The standalone mode ensures the project can be built and demonstrated on any machine, while the plugin mode provides the production-quality integration.

## 3. Validation Passes

Four passes run sequentially on each inline asm block:

### Pass 1: Register Name Validation
- Tokenize assembly operands; any operand classified as a register is checked against a comprehensive set of valid x86-64 register names (GPRs, aliases, segment, XMM, YMM).
- **Severity:** Error — an invalid register name is always a bug.

### Pass 2: Clobber Declaration Check
- For "write" mnemonics (mov, add, sub, xor, lea, pop, etc.), the AT&T destination operand (last operand) is identified.
- If the destination is a caller-saved register and not listed in the clobber annotation, a warning is emitted.
- **Severity:** Warning — the compiler may or may not have a live value in that register.

### Pass 3: Stack Alignment Analysis
- Tracks a cumulative `stackDelta` counter: +8 per push, -8 per pop, +N for `sub $N, %rsp`.
- If the delta is nonzero at the end of the asm block, the stack is unbalanced.
- **Severity:** Warning — violates the 16-byte alignment contract of the System V AMD64 ABI.

### Pass 4: Callee-Saved Register Check
- Tracks which callee-saved registers (rbx, rbp, r12-r15) are pushed (saved) and popped (restored).
- If a callee-saved register is written without being saved or declared as clobber, an error is emitted.
- If a register is pushed but never popped, an error is emitted (stack leak).
- **Severity:** Error — ABI violation that corrupts the caller's state.

## 4. Tokenizer Design

The assembly tokenizer processes AT&T-syntax strings:

```
Input:  "pushq %%rbx\n\tmovq $42, %%rbx\n\tpopq %%rbx"
                        |
                  Split on \n and ;
                        |
          ┌─────────────┼─────────────┐
    "pushq %%rbx"  "movq $42, %%rbx"  "popq %%rbx"
          |              |                   |
     Parse mnemonic + operands          Parse mnemonic + operands
          |              |                   |
   {push, [rbx]}  {mov, [$42, rbx]}   {pop, [rbx]}
```

Operand classification:
- `%reg` or `%%reg` → Register (strip prefix, lowercase)
- `$imm` or digit → Immediate
- `(...)` or `[...]` → Memory
- Known register name → Register (Intel syntax fallback)
- `%0`, `%1`, `%[name]` → GCC placeholder (skipped)
- Anything else → Unknown

## 5. Alternative Solutions Considered

### 5.1 LLVM IR-Level Analysis
- **Approach:** Write an LLVM pass that inspects `InlineAsm` IR nodes after lowering.
- **Pros:** Access to constraint resolution and register allocation context.
- **Cons:** Much higher complexity; inline asm strings are still opaque at IR level; constraints are already resolved into register classes, losing the original register names.
- **Decision:** Rejected — AST-level analysis provides direct access to the raw assembly string and source-level clobber annotations.

### 5.2 clang-tidy Check
- **Approach:** Implement the validator as a `clang-tidy` check using AST matchers.
- **Pros:** Automatic integration with CI/CD pipelines and IDE linters.
- **Cons:** Requires building against clang-tidy's infrastructure, which is heavier than a simple plugin. Limited control over diagnostic formatting.
- **Decision:** Deferred as a future enhancement; the core logic is identical and can be ported.

### 5.3 External Static Analyzer (e.g., Cppcheck Plugin)
- **Approach:** Write a Cppcheck addon that regex-matches `asm(` blocks.
- **Pros:** No LLVM dependency.
- **Cons:** No AST access — purely text-based. Cannot access constraint annotations or clobber lists reliably. Many false positives from macro expansions.
- **Decision:** Rejected — the standalone mode already provides text-based analysis with better accuracy.

### 5.4 Full x86 Decoder (e.g., Capstone/Zydis)
- **Approach:** Disassemble the inline asm with a real x86 decoder library.
- **Pros:** Complete instruction validation including operand sizes, prefixes, and encoding.
- **Cons:** Inline asm contains GCC operand placeholders (`%0`, `%[name]`) that are not valid machine code. Would require placeholder resolution before decoding.
- **Decision:** Rejected for this scope — a lightweight tokenizer is sufficient for the four target checks.

## 6. Design Trade-offs

| Decision | Trade-off |
|----------|-----------|
| AT&T syntax only | Simplifies tokenizer; Intel syntax requires a parallel parsing path |
| Heuristic write-mnemonic set | May miss exotic instructions; easily extensible |
| Linear scan (no CFG) | Cannot handle conditional clobbers or jumps within asm blocks |
| Static clobber set comparison | Cannot detect clobbers declared via output constraints (`"=r"`) for non-explicit registers |
| Plugin `AddAfterMainAction` | Runs after type checking, so the AST is fully resolved, but adds to compile time |

## 7. Extensibility

The design is modular and supports future enhancements:
- **Intel syntax support** — Add a syntax detector and parallel tokenizer
- **Mnemonic validation** — Extend with a valid opcode set to catch typos like `movx`
- **Fix-it hints** — Use Clang's `FixItHint` API to suggest adding missing clobbers
- **clang-tidy integration** — Wrap the validation logic in a `clang-tidy` check
- **SIMD register coverage** — Add ZMM0-31 and AVX-512 mask registers
