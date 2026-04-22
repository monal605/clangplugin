# Implementation Details — LLVM/Clang-Specific

## 1. Plugin Architecture

The validator is implemented as a **Clang frontend plugin** that loads into the compiler at parse time and runs after the AST is fully built.

### 1.1 Plugin Registration

```cpp
static FrontendPluginRegistry::Add<AsmValidatorAction>
    X("asm-validator", "Validates x86-64 inline assembly");
```

This macro registers the plugin with Clang's plugin registry. When loaded via `-fplugin=./build/libAsmValidator.so`, the plugin is discovered by name and instantiated automatically.

### 1.2 Class Hierarchy

```
PluginASTAction                    (Clang base class)
  └── AsmValidatorAction           (plugin entry point)
        ├── CreateASTConsumer()     → returns AsmValidatorConsumer
        ├── ParseArgs()             → accepts all arguments
        └── getActionType()         → AddAfterMainAction

ASTConsumer                        (Clang base class)
  └── AsmValidatorConsumer
        └── HandleTranslationUnit() → launches the visitor

RecursiveASTVisitor<T>             (Clang CRTP base)
  └── AsmValidatorVisitor
        └── VisitGCCAsmStmt()      → extracts asm string, runs 4 passes
```

### 1.3 Action Type

The plugin uses `AddAfterMainAction` so it executes **after** the main compilation action. This ensures the full AST (with resolved types and templates) is available, but does not block compilation.

## 2. AST Node: GCCAsmStmt

Clang represents GCC-style inline assembly as `GCCAsmStmt` AST nodes. Key APIs used:

| API | Purpose |
|-----|---------|
| `S->getAsmString()` | Returns the `StringLiteral*` containing the raw assembly text |
| `SL->getString().str()` | Converts the Clang `StringRef` to `std::string` |
| `S->getNumClobbers()` | Returns the number of clobber declarations |
| `S->getClobber(i)` | Returns the i-th clobber as a `StringLiteral*` |
| `S->getAsmLoc()` | Returns the `SourceLocation` of the `asm` keyword |

### 2.1 Asm String Extraction

```cpp
std::string asmStr;
if (auto* SL = S->getAsmString()) {
    asmStr = SL->getString().str();
}
```

The string literal returned by `getAsmString()` contains the raw assembly text **after** string literal concatenation and escape processing by the C preprocessor. GCC operand placeholders (`%0`, `%1`, `%[name]`) are still present as literal `%` characters.

### 2.2 Clobber Extraction

```cpp
std::unordered_set<std::string> declaredClobbers;
for (unsigned i = 0; i < S->getNumClobbers(); ++i) {
    std::string clobber = S->getClobber(i)->getString().str();
    std::transform(clobber.begin(), clobber.end(),
                   clobber.begin(), ::tolower);
    declaredClobbers.insert(clobber);
}
```

Clobbers are stored as individual `StringLiteral` nodes in the AST. They are extracted, lowercased for case-insensitive comparison, and stored in an `unordered_set` for O(1) lookup during validation.

## 3. Diagnostics Engine

All diagnostics are emitted through Clang's `DiagnosticsEngine`, which provides:
- Native integration with compiler output formatting
- Support for `-Werror`, `-w`, and other diagnostic flags
- Source location display (file:line:column)
- IDE error-parser compatibility

### 3.1 Custom Diagnostic IDs

```cpp
unsigned getDiagID(DiagnosticsEngine::Level lvl, const char* msg) {
    return Diags.getCustomDiagID(lvl, msg);
}
```

Each validation pass creates custom diagnostic IDs on the fly using `getCustomDiagID()`. This avoids the need for static diagnostic tables (which would require modifying Clang's `DiagnosticSemaKinds.td`).

### 3.2 Diagnostic Emission

```cpp
auto diagID = getDiagID(DiagnosticsEngine::Error,
    "inline asm: unknown x86-64 register '%%%0'");
Diags.Report(S->getAsmLoc(), diagID) << op.value;
```

- `S->getAsmLoc()` provides the source location of the `asm` keyword
- `%0` in the message template is replaced by the first argument (`op.value`)
- The `%%` literal produces a single `%` in the output (for AT&T register prefix)

### 3.3 Severity Levels Used

| Level | When Used |
|-------|-----------|
| `DiagnosticsEngine::Error` | Invalid register name; callee-saved ABI violation |
| `DiagnosticsEngine::Warning` | Missing clobber declaration; stack imbalance |

## 4. Tokenizer Implementation

### 4.1 File Organization

| File | Role |
|------|------|
| `AsmTokenizer.h` | Defines `AsmOperand`, `AsmInstruction`, and `AsmTokenizer` class |
| `AsmTokenizer.cpp` | Implements tokenization: line splitting, mnemonic extraction, operand classification |
| `RegisterTable.h` | Defines `VALID_REGISTERS`, `CALLER_SAVED`, `CALLEE_SAVED` as global `unordered_set<string>` |

### 4.2 Data Structures

```cpp
struct AsmOperand {
    enum Kind { Register, Immediate, Memory, Label, Unknown };
    Kind kind;
    std::string value;  // e.g., "rax", "42", "[rsp+8]"
};

struct AsmInstruction {
    std::string mnemonic;              // e.g., "mov", "push"
    std::vector<AsmOperand> operands;
    int lineOffset;                    // line within the asm block
};
```

### 4.3 Tokenization Pipeline

1. Split input on `\n` to get lines
2. Split each line on `;` to get statements
3. Strip leading whitespace
4. Skip labels (ending with `:`) and directives (starting with `.`)
5. First token = mnemonic (lowercased)
6. Remaining tokens = operands (comma-stripped, classified by prefix)

## 5. Register Tables

### 5.1 VALID_REGISTERS (72 entries)

Covers all architectural x86-64 register names:
- 64-bit GPRs: rax, rbx, ..., r15
- 32-bit aliases: eax, ebx, ..., r15d
- 16-bit aliases: ax, bx, ..., bp
- 8-bit aliases: al, ah, ..., bpl
- Segment: cs, ds, es, fs, gs, ss
- Special: rip, rflags
- SIMD: xmm0-xmm15, ymm0-ymm7

### 5.2 CALLER_SAVED (System V AMD64)

Volatile registers that may be freely clobbered by any function call:
`rax, rcx, rdx, rsi, rdi, r8-r11, xmm0-xmm7`

### 5.3 CALLEE_SAVED (System V AMD64)

Non-volatile registers that must be preserved across calls:
`rbx, rbp, r12-r15`

## 6. Build System

### 6.1 CMakeLists.txt (Plugin)

```cmake
find_package(LLVM REQUIRED CONFIG)
find_package(Clang REQUIRED CONFIG)
add_library(AsmValidator SHARED
    plugin/AsmValidator.cpp
    plugin/AsmTokenizer.cpp
)
target_compile_options(AsmValidator PRIVATE -fno-rtti)
target_link_libraries(AsmValidator PRIVATE clangAST clangFrontend clangBasic)
```

Key points:
- `-fno-rtti` is required because LLVM is built without RTTI
- Links against `clangAST` (for `GCCAsmStmt`), `clangFrontend` (for `PluginASTAction`), and `clangBasic` (for `DiagnosticsEngine`)
- Produces `libAsmValidator.so` (Linux) or `AsmValidator.dylib` (macOS)

### 6.2 Standalone Build

The standalone binary compiles with a single command:
```bash
g++ -std=c++17 -O2 -o asm_validator standalone/AsmValidatorStandalone.cpp
```

No external dependencies beyond the C++17 standard library.

## 7. Standalone Mode Implementation

The standalone validator (`AsmValidatorStandalone.cpp`) replicates the plugin's functionality without LLVM:

| Feature | Plugin | Standalone |
|---------|--------|------------|
| AST access | `GCCAsmStmt` node | Text-based `asm(` pattern matching |
| String extraction | `getAsmString()` | Parenthesis-matching + string literal parsing |
| Clobber extraction | `getClobber(i)` | Colon-delimited section parsing |
| `%%` handling | Not needed (Clang resolves) | Strips double `%%` to single `%` |
| GCC placeholders | Not in raw string | Detected and skipped (`%0`, `%[name]`) |
| Source location | Clang `SourceLocation` | Line counting from file start |
| Diagnostics | `DiagnosticsEngine` | `stdout` with `file:line: level: message` format |

The standalone mode handles real-world patterns like concatenated string literals with comments between them, `__asm__` and `__volatile__` variants, and nested parentheses.
