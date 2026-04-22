# Code Explanation

This document explains every tracked source file in the repository and gives a line-by-line walkthrough of what the code is doing.

## Repository at a Glance

- `CMakeLists.txt`: Builds the Clang plugin.
- `plugin/AsmValidator.cpp`: Main plugin implementation and validation passes.
- `plugin/AsmTokenizer.h`: Tokenizer data structures and API.
- `plugin/AsmTokenizer.cpp`: Tokenizer implementation.
- `plugin/RegisterTable.h`: Register lookup tables.
- `plugin/CallingConvention.h`: ABI notes and constants.
- `tests/*.c`: Positive and negative inline-asm examples.
- `run_tests.sh`: Script that runs the plugin against the tests.
- `REPORT.md`: Project report and design summary.

## `CMakeLists.txt`

Line 1: Requires CMake 3.16 or newer.

Line 2: Names the project `AsmValidator`.

Line 3: Blank line for readability.

Line 4: Finds the LLVM package and makes it required.

Line 5: Finds the Clang package and makes it required.

Line 6: Blank line before build configuration.

Line 7: Adds LLVM and Clang include directories globally.

Line 8: Adds LLVM compile definitions globally.

Line 9: Blank line before target creation.

Line 10: Starts defining a shared library target named `AsmValidator`.

Line 11: Adds `plugin/AsmValidator.cpp` to the target.

Line 12: Adds `plugin/AsmTokenizer.cpp` to the target.

Line 13: Closes the library definition.

Line 14: Blank line before target-specific options.

Line 15: Disables RTTI for the plugin target.

Line 16: Links the plugin against the Clang libraries it uses.

## `plugin/AsmTokenizer.h`

Line 1: File comment naming the header.

Line 2: `#pragma once` prevents duplicate inclusion.

Line 3: Includes `std::string`.

Line 4: Includes `std::vector`.

Line 5: Blank line before declarations.

Line 6: Starts the `AsmOperand` structure.

Line 7: Declares operand categories with an inline enum.

Line 8: Stores the detected operand kind.

Line 9: Stores the textual operand value.

Line 10: Ends `AsmOperand`.

Line 11: Blank line between related structs.

Line 12: Starts the `AsmInstruction` structure.

Line 13: Stores the instruction mnemonic.

Line 14: Stores all parsed operands.

Line 15: Stores the source-line offset within the asm block.

Line 16: Ends `AsmInstruction`.

Line 17: Blank line before the tokenizer class.

Line 18: Declares the `AsmTokenizer` class.

Line 19: Opens the public section.

Line 20: Comment saying the parser expects AT&T syntax.

Line 21: Comment listing supported instruction separators.

Line 22: Declares the main tokenization API.

Line 23: Blank line before the private helpers.

Line 24: Opens the private section.

Line 25: Declares a helper that parses one statement line.

Line 26: Declares a helper that parses one operand token.

Line 27: Ends the class definition.

## `plugin/AsmTokenizer.cpp`

Line 1: File comment naming the implementation file.

Line 2: Includes the matching header.

Line 3: Includes register tables used during classification.

Line 4: Includes `sstream` for line and token parsing.

Line 5: Includes algorithms such as `std::transform`.

Line 6: Includes character helpers like `std::isdigit`.

Line 7: Blank line before function definitions.

Line 8: Defines the tokenizer entry point.

Line 9: Creates the result vector.

Line 10: Wraps the asm text in an input stream.

Line 11: Declares a buffer for each source line.

Line 12: Initializes a line counter.

Line 13: Reads the asm block one newline-separated line at a time.

Line 14: Comment noting that semicolons are also treated as separators.

Line 15: Wraps the current line in another stream.

Line 16: Declares a buffer for each semicolon-separated statement.

Line 17: Reads each statement from the current line.

Line 18: Comment for whitespace trimming.

Line 19: Finds the first non-space character.

Line 20: Skips empty statement fragments.

Line 21: Removes leading whitespace.

Line 22: Comment for directive and label skipping.

Line 23: Skips directives, labels, and empty statements.

Line 24: Parses and stores the statement as an instruction.

Line 25: Ends the semicolon-splitting loop.

Line 26: Increments the line counter.

Line 27: Ends the newline loop.

Line 28: Returns the parsed instructions.

Line 29: Ends `tokenize`.

Line 30: Blank line before `parseLine`.

Line 31: Defines the single-line parser.

Line 32: Creates the output instruction object.

Line 33: Stores the input line number.

Line 34: Wraps the line in a string stream.

Line 35: Reads the first token as the mnemonic.

Line 36: Comment for lowercase normalization.

Line 37: Starts lowercasing the mnemonic.

Line 38: Finishes lowercasing the mnemonic.

Line 39: Declares an operand token buffer.

Line 40: Reads remaining whitespace-separated tokens.

Line 41: Comment for comma stripping.

Line 42: Checks whether a token ends with a comma.

Line 43: Removes the comma if present.

Line 44: Parses and stores the operand.

Line 45: Ends the operand loop.

Line 46: Returns the completed instruction.

Line 47: Ends `parseLine`.

Line 48: Blank line before `parseOperand`.

Line 49: Defines operand parsing.

Line 50: Creates the output operand object.

Line 51: Stores the raw token as the default value.

Line 52: Detects AT&T-style register tokens beginning with `%`.

Line 53: Comment explaining that `%` is being stripped.

Line 54: Removes the leading `%`.

Line 55: Lowercases the stored register spelling.

Line 56: Marks the operand as a register.

Line 57: Otherwise checks for immediate-like operands.

Line 58: Marks the operand as an immediate.

Line 59: Otherwise checks for memory-style syntax.

Line 60: Marks the operand as memory.

Line 61: Otherwise checks for an exact known register name.

Line 62: Stores the exact token as the value.

Line 63: Marks the operand as a register.

Line 64: Handles every other token as unknown.

Line 65: Marks the operand as unknown.

Line 66: Ends the classification chain.

Line 67: Returns the operand object.

Line 68: Ends `parseOperand`.

## `plugin/RegisterTable.h`

Line 1: File comment naming the header.

Line 2: `#pragma once` prevents duplicate inclusion.

Line 3: Includes `std::unordered_set`.

Line 4: Includes `std::string`.

Line 5: Blank line before the first table.

Line 6: Comment describing the valid-register table.

Line 7: Starts the global set of accepted x86-64 register spellings.

Line 8: Comment for 64-bit general-purpose registers.

Line 9: Adds `rax`, `rbx`, `rcx`, `rdx`, `rsi`, and `rdi`.

Line 10: Adds `rsp`, `rbp`, `r8`, `r9`, `r10`, and `r11`.

Line 11: Adds `r12`, `r13`, `r14`, and `r15`.

Line 12: Comment for 32-bit aliases.

Line 13: Adds classic 32-bit aliases such as `eax`.

Line 14: Adds more 32-bit aliases including extended-register forms.

Line 15: Adds remaining 32-bit aliases for `r12d` through `r15d`.

Line 16: Comment for 16-bit aliases.

Line 17: Adds 16-bit aliases like `ax` and `bp`.

Line 18: Comment for 8-bit aliases.

Line 19: Adds 8-bit aliases such as `al`, `ah`, `bl`, and `bh`.

Line 20: Adds additional low-byte names such as `spl` and `bpl`.

Line 21: Comment for segment and related names.

Line 22: Adds segment registers plus `rip` and `rflags`.

Line 23: Comment for vector-register coverage.

Line 24: Adds `xmm0` through `xmm5`.

Line 25: Adds `xmm6` through `xmm11`.

Line 26: Adds `xmm12` through `xmm15`.

Line 27: Adds `ymm0` through `ymm5`.

Line 28: Adds `ymm6` and `ymm7`.

Line 29: Ends the valid-register set.

Line 30: Blank line between tables.

Line 31: Comment introducing ABI classification.

Line 32: Comment explaining caller-saved registers.

Line 33: Starts the caller-saved set.

Line 34: Adds volatile integer registers `rax`, `rcx`, `rdx`, `rsi`, and `rdi`.

Line 35: Adds volatile extended integer registers `r8` through `r11`.

Line 36: Adds volatile vector registers `xmm0` through `xmm3`.

Line 37: Adds `xmm4` through `xmm7`.

Line 38: Ends the caller-saved set.

Line 39: Blank line before the next set.

Line 40: Comment explaining callee-saved registers.

Line 41: Starts the callee-saved set.

Line 42: Adds `rbx`, `rbp`, and `r12` through `r15`.

Line 43: Ends the callee-saved set.

## `plugin/CallingConvention.h`

Line 1: File comment naming the header.

Line 2: `#pragma once` prevents multiple inclusion.

Line 3: Comment spacer line.

Line 4: Announces that the header documents the System V AMD64 ABI.

Line 5: Comment spacer line.

Line 6: Explains that this header provides ABI notes and constants.

Line 7: Finishes the note about target operating systems.

Line 8: Comment spacer line.

Line 9: Introduces caller-saved registers in the comment block.

Line 10: Lists the caller-saved integer and vector registers.

Line 11: Explains that callees may overwrite these freely.

Line 12: Explains the caller's responsibility to save them when needed.

Line 13: Comment spacer line.

Line 14: Introduces callee-saved registers.

Line 15: Lists the callee-saved integer registers.

Line 16: States that these must be preserved across calls.

Line 17: Connects that rule to inline asm blocks.

Line 18: Comment spacer line.

Line 19: Introduces stack-alignment notes.

Line 20: States the 16-byte alignment requirement before `call`.

Line 21: Explains the common `rsp % 16 == 8` state at function entry.

Line 22: Notes that prologue code may need to realign the stack.

Line 23: Comment spacer line.

Line 24: Introduces return-value conventions.

Line 25: Lists integer and pointer return registers.

Line 26: Lists floating-point return registers.

Line 27: Comment spacer line.

Line 28: Introduces integer and pointer argument registers.

Line 29: Lists their passing order.

Line 30: Comment spacer line.

Line 31: Introduces floating-point argument registers.

Line 32: Lists their passing order.

Line 33: Ends the ABI comment block.

Line 34: Blank line before includes.

Line 35: Includes `std::string`, even though this header does not currently use it directly.

Line 36: Blank line before the namespace.

Line 37: Opens namespace `abi`.

Line 38: Blank line before constants.

Line 39: Declares a named constant for stack alignment.

Line 40: Declares a named constant for 64-bit `push` and `pop` size.

Line 41: Blank line before closing the namespace.

Line 42: Closes namespace `abi`.

## `plugin/AsmValidator.cpp`

Line 1: File comment naming the main implementation file.

Line 2: Describes the file as a Clang plugin for inline-asm validation.

Line 3: Explains that the plugin hooks into the AST and visits `GCCAsmStmt`.

Line 4: Introduces the list of validation categories.

Line 5: Names invalid-register detection.

Line 6: Names missing-clobber detection.

Line 7: Names stack-misalignment detection.

Line 8: Names callee-saved register corruption detection.

Line 9: Blank line before includes.

Line 10: Includes the plugin-registration API.

Line 11: Includes `ASTConsumer`.

Line 12: Includes `RecursiveASTVisitor`.

Line 13: Includes `CompilerInstance`.

Line 14: Includes statement AST declarations.

Line 15: Includes diagnostic support.

Line 16: Includes source-location support.

Line 17: Includes the tokenizer header from this project.

Line 18: Includes the register tables from this project.

Line 19: Includes `std::unordered_set`.

Line 20: Includes `std::string`.

Line 21: Blank line before namespace usage.

Line 22: Imports the `clang` namespace for shorter type names.

Line 23: Blank line before the visitor banner.

Line 24: Decorative separator comment.

Line 25: States that the next class is the AST visitor.

Line 26: Decorative separator comment.

Line 27: Starts class `AsmValidatorVisitor`.

Line 28: Inherits from `RecursiveASTVisitor` using CRTP.

Line 29: Opens the public section.

Line 30: Declares the constructor taking a `CompilerInstance`.

Line 31: Initializes the compiler-instance and diagnostics-engine references.

Line 32: Initializes the source-manager reference.

Line 33: Blank line before the visit method.

Line 34: Defines the callback used for every `GCCAsmStmt`.

Line 35: Comment for asm-string extraction.

Line 36: Creates a local `std::string` to store the asm text.

Line 37: Tries to fetch the asm string literal from Clang.

Line 38: Copies the string literal into `asmStr`.

Line 39: Starts the fallback path when no string is available.

Line 40: Returns early and skips validation.

Line 41: Ends the asm-string branch.

Line 42: Blank line before clobber extraction.

Line 43: Comment for collecting clobbers.

Line 44: Creates a set for normalized clobber names.

Line 45: Iterates over all declared clobbers.

Line 46: Copies the current clobber into a mutable string.

Line 47: Starts lowercasing that clobber name.

Line 48: Finishes lowercasing it.

Line 49: Inserts it into the set.

Line 50: Ends the clobber loop.

Line 51: Blank line before tokenization.

Line 52: Comment for asm tokenization.

Line 53: Converts the raw asm text into parsed instructions.

Line 54: Blank line before validations.

Line 55: Comment for the validation phase.

Line 56: Runs register-name validation.

Line 57: Runs clobber validation.

Line 58: Runs stack-alignment validation.

Line 59: Runs callee-saved validation.

Line 60: Blank line before the return.

Line 61: Returns `true` so AST traversal keeps going.

Line 62: Ends `VisitGCCAsmStmt`.

Line 63: Blank line before the private section.

Line 64: Opens the private section.

Line 65: Stores the compiler instance.

Line 66: Stores the diagnostics engine.

Line 67: Stores the source manager.

Line 68: Blank line before helper methods.

Line 69: Decorative separator comment for diagnostic helpers.

Line 70: Defines a helper that requests a custom diagnostic ID.

Line 71: Returns the custom ID from Clang.

Line 72: Ends the helper function.

Line 73: Blank line before pass 1.

Line 74: Decorative separator comment for register validation.

Line 75: Starts the `validateRegisters` function signature.

Line 76: Finishes the parameter list with parsed instructions.

Line 77: Loops over each instruction.

Line 78: Loops over each operand of the instruction.

Line 79: Checks whether the operand kind is `Register`.

Line 80: Also checks whether that register name is not in `VALID_REGISTERS`.

Line 81: Starts building an error diagnostic.

Line 82: Uses error severity.

Line 83: Provides the message template.

Line 84: Emits the diagnostic at the asm location.

Line 85: Inserts the bad register name into the diagnostic.

Line 86: Ends the diagnostic block.

Line 87: Ends the operand loop.

Line 88: Ends the instruction loop.

Line 89: Ends `validateRegisters`.

Line 90: Blank line before pass 2.

Line 91: Decorative separator comment for clobber validation.

Line 92: Starts the `validateClobbers` function signature.

Line 93: Continues the parameter list.

Line 94: Adds the declared-clobber set parameter.

Line 95: Comment explaining the write-mnemonic heuristic.

Line 96: Starts a static set of mnemonics treated as writing a destination operand.

Line 97: Adds move-family mnemonics.

Line 98: Adds arithmetic and multiply/divide mnemonics.

Line 99: Adds logical and unary mnemonics.

Line 100: Adds address-generation and pop mnemonics.

Line 101: Ends the write-mnemonic set.

Line 102: Loops over each instruction.

Line 103: Skips mnemonics not in the write set.

Line 104: Comment noting the AT&T assumption that destination is last.

Line 105: Skips instructions with no operands.

Line 106: Uses the last operand as the destination candidate.

Line 107: Skips non-register destinations.

Line 108: Skips non-caller-saved registers because another pass handles callee-saved ones.

Line 109: Checks whether the destination register is missing from the declared clobbers.

Line 110: Starts building a warning diagnostic.

Line 111: Uses warning severity.

Line 112: Provides the warning message.

Line 113: Emits the warning at the asm location.

Line 114: Inserts the destination register name.

Line 115: Ends the diagnostic block.

Line 116: Ends the instruction loop.

Line 117: Ends `validateClobbers`.

Line 118: Blank line before pass 3.

Line 119: Decorative separator comment for stack analysis.

Line 120: Starts the `validateStackAlignment` function signature.

Line 121: Finishes the parameter list.

Line 122: Initializes the net stack delta to zero.

Line 123: Loops over each instruction.

Line 124: Detects `push` and `pushq`.

Line 125: Adds 8 bytes for a push.

Line 126: Detects `pop` and `popq`.

Line 127: Subtracts 8 bytes for a pop.

Line 128: Detects `sub` or `subq`.

Line 129: Ensures there is at least one operand.

Line 130: Checks whether the destination register is `rsp`.

Line 131: Comment explaining the immediate parse attempt.

Line 132: Requires at least two operands.

Line 133: Requires the first operand to be an immediate.

Line 134: Starts a `try` block.

Line 135: Copies the immediate text into a temporary variable.

Line 136: Removes a leading `$` if present.

Line 137: Adds the parsed integer amount to `stackDelta`.

Line 138: Silently ignores parse failures.

Line 139: Ends the immediate-extraction branch.

Line 140: Ends the `sub` branch.

Line 141: Ends the instruction loop.

Line 142: Checks whether the stack delta is nonzero.

Line 143: Starts building a warning diagnostic.

Line 144: Uses warning severity.

Line 145: Provides the first half of the warning text.

Line 146: Provides the second half of the warning text.

Line 147: Emits the warning and inserts the byte delta.

Line 148: Ends the diagnostic block.

Line 149: Ends `validateStackAlignment`.

Line 150: Blank line before pass 4.

Line 151: Decorative separator comment for callee-saved validation.

Line 152: Starts the `validateCalleeSaved` function signature.

Line 153: Continues the parameter list.

Line 154: Adds the declared-clobber set parameter.

Line 155: Starts the write-mnemonic set for this pass.

Line 156: Adds move-family mnemonics.

Line 157: Adds arithmetic and logical mnemonics.

Line 158: Adds more unary and address-generation mnemonics.

Line 159: Ends the write-mnemonic set.

Line 160: Creates sets to track saved and restored callee-saved registers.

Line 161: Loops over each instruction.

Line 162: Detects `push` and `pushq`.

Line 163: Requires an operand.

Line 164: Checks whether the pushed operand is callee-saved.

Line 165: Records that register as saved.

Line 166: Otherwise detects `pop` and `popq`.

Line 167: Requires an operand.

Line 168: Checks whether the popped operand is callee-saved.

Line 169: Records that register as restored.

Line 170: Otherwise checks whether the instruction writes a destination operand.

Line 171: Requires at least one operand.

Line 172: Uses the last operand as the destination candidate.

Line 173: Requires the destination to be a register.

Line 174: Requires it to be callee-saved.

Line 175: Requires that it was not previously saved.

Line 176: Requires that it was also not listed as clobbered.

Line 177: Starts building an error diagnostic.

Line 178: Uses error severity.

Line 179: Provides the first half of the error message.

Line 180: Provides the second half of the error message.

Line 181: Emits the error and inserts the register name.

Line 182: Ends the diagnostic block.

Line 183: Ends the inner branch.

Line 184: Ends the outer branch.

Line 185: Ends the instruction loop.

Line 186: Comment introducing the unmatched-save check.

Line 187: Loops over all saved registers.

Line 188: Checks whether each one was never restored.

Line 189: Starts building another error diagnostic.

Line 190: Uses error severity.

Line 191: Provides the first half of the message.

Line 192: Provides the second half of the message.

Line 193: Emits the error and inserts the register name.

Line 194: Ends the diagnostic block.

Line 195: Ends the saved-register loop.

Line 196: Ends `validateCalleeSaved`.

Line 197: Ends the visitor class.

Line 198: Blank line before the consumer section.

Line 199: Decorative separator comment.

Line 200: States that the next class is the AST consumer.

Line 201: Decorative separator comment.

Line 202: Starts class `AsmValidatorConsumer`.

Line 203: Stores a visitor as a member.

Line 204: Opens the public section.

Line 205: Defines the constructor that initializes the visitor.

Line 206: Overrides `HandleTranslationUnit`.

Line 207: Traverses the translation unit starting from the root declaration.

Line 208: Ends `HandleTranslationUnit`.

Line 209: Ends the consumer class.

Line 210: Blank line before the frontend-action section.

Line 211: Decorative separator comment.

Line 212: States that the next class is the plugin entry point.

Line 213: Decorative separator comment.

Line 214: Starts class `AsmValidatorAction`, derived from `PluginASTAction`.

Line 215: Opens the protected section.

Line 216: Starts the return type for `CreateASTConsumer`.

Line 217: Overrides `CreateASTConsumer`.

Line 218: Returns a new `AsmValidatorConsumer`.

Line 219: Ends `CreateASTConsumer`.

Line 220: Starts the `ParseArgs` override.

Line 221: Finishes the parameter list.

Line 222: Accepts all plugin arguments by always returning `true`.

Line 223: Ends `ParseArgs`.

Line 224: Overrides `getActionType`.

Line 225: Requests execution after the main action.

Line 226: Ends `getActionType`.

Line 227: Ends the action class.

Line 228: Blank line before registration.

Line 229: Decorative separator comment.

Line 230: States that the next line registers the plugin.

Line 231: Decorative separator comment.

Line 232: Instantiates static registration glue for the plugin action.

Line 233: Gives the plugin its command-line name and description.
