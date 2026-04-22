// AsmValidatorStandalone.cpp
// Standalone x86-64 inline assembly validator for Windows.
// No LLVM/Clang libraries needed — compiles with MSVC, MinGW, or g++.
//
// Usage:  asm_validator.exe <file.c> [file2.c ...]
//
// Parses C/C++ source files, extracts GCC-style inline asm blocks,
// tokenizes the assembly, and runs 4 validation passes:
//   1. Register name validation
//   2. Clobber declaration check
//   3. Stack alignment analysis
//   4. Callee-saved register check

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include <regex>

// ═══════════════════════════════════════════════════════════
// Register Tables
// ═══════════════════════════════════════════════════════════

const std::unordered_set<std::string> VALID_REGISTERS = {
    // 64-bit GPRs
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi",
    "rsp", "rbp", "r8",  "r9",  "r10", "r11",
    "r12", "r13", "r14", "r15",
    // 32-bit aliases
    "eax", "ebx", "ecx", "edx", "esi", "edi",
    "esp", "ebp", "r8d", "r9d", "r10d","r11d",
    "r12d","r13d","r14d","r15d",
    // 16-bit aliases
    "ax","bx","cx","dx","si","di","sp","bp",
    // 8-bit aliases
    "al","ah","bl","bh","cl","ch","dl","dh",
    "sil","dil","spl","bpl",
    // Segment / control
    "cs","ds","es","fs","gs","ss","rip","rflags",
    // XMM / YMM
    "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5",
    "xmm6","xmm7","xmm8","xmm9","xmm10","xmm11",
    "xmm12","xmm13","xmm14","xmm15",
    "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5",
    "ymm6","ymm7"
};

const std::unordered_set<std::string> CALLER_SAVED = {
    "rax","rcx","rdx","rsi","rdi",
    "r8","r9","r10","r11",
    // 32-bit aliases
    "eax","ecx","edx","esi","edi",
    "r8d","r9d","r10d","r11d",
    // 16-bit aliases
    "ax","cx","dx","si","di",
    // 8-bit aliases
    "al","ah","cl","ch","dl","dh","sil","dil",
    // XMM
    "xmm0","xmm1","xmm2","xmm3",
    "xmm4","xmm5","xmm6","xmm7"
};

const std::unordered_set<std::string> CALLEE_SAVED = {
    "rbx","rbp","r12","r13","r14","r15",
    // 32-bit aliases
    "ebx","ebp","r12d","r13d","r14d","r15d"
};

const std::unordered_set<std::string> WRITE_MNEMONICS = {
    "mov","movq","movl","movw","movb",
    "add","addq","sub","subq","imul","idiv",
    "xor","xorq","or","and","not","neg",
    "lea","leaq","pop","popq"
};

// ═══════════════════════════════════════════════════════════
// Tokenizer
// ═══════════════════════════════════════════════════════════

struct AsmOperand {
    enum Kind { Register, Immediate, Memory, Label, Unknown };
    Kind kind;
    std::string value;
};

struct AsmInstruction {
    std::string mnemonic;
    std::vector<AsmOperand> operands;
    int lineOffset;
};

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

// Check if a string is a GCC operand placeholder like %0, %1, %[name]
bool isOperandPlaceholder(const std::string& val) {
    if (val.empty()) return false;
    // Numeric placeholders: 0, 1, 2, ...
    if (std::isdigit(val[0])) return true;
    // Named placeholders: [name]
    if (val[0] == '[') return true;
    return false;
}

AsmOperand parseOperand(const std::string& token) {
    AsmOperand op;
    op.value = token;
    if (token.size() > 1 && token[0] == '%') {
        // AT&T register: strip '%' or '%%'
        std::string regName;
        if (token.size() > 2 && token[1] == '%')
            regName = token.substr(2);
        else
            regName = token.substr(1);
        regName = toLower(regName);
        // Skip GCC operand placeholders like %0, %1, %[name]
        if (isOperandPlaceholder(regName)) {
            op.kind = AsmOperand::Unknown;
            return op;
        }
        op.value = regName;
        op.kind = AsmOperand::Register;
    } else if (!token.empty() && (token[0] == '$' || std::isdigit(token[0]) || token[0] == '-')) {
        op.kind = AsmOperand::Immediate;
    } else if (!token.empty() && (token[0] == '[' || token.find('(') != std::string::npos)) {
        op.kind = AsmOperand::Memory;
    } else if (VALID_REGISTERS.count(toLower(token))) {
        op.value = toLower(token);
        op.kind = AsmOperand::Register;
    } else {
        op.kind = AsmOperand::Unknown;
    }
    return op;
}

AsmInstruction parseLine(const std::string& line, int lineNum) {
    AsmInstruction instr;
    instr.lineOffset = lineNum;
    std::istringstream ss(line);
    ss >> instr.mnemonic;
    instr.mnemonic = toLower(instr.mnemonic);
    std::string token;
    while (ss >> token) {
        if (!token.empty() && token.back() == ',')
            token.pop_back();
        if (!token.empty())
            instr.operands.push_back(parseOperand(token));
    }
    return instr;
}

std::vector<AsmInstruction> tokenize(const std::string& asmStr) {
    std::vector<AsmInstruction> result;
    std::istringstream stream(asmStr);
    std::string line;
    int lineNum = 0;
    while (std::getline(stream, line, '\n')) {
        std::istringstream lineStream(line);
        std::string stmt;
        while (std::getline(lineStream, stmt, ';')) {
            stmt = trim(stmt);
            if (stmt.empty() || stmt[0] == '.' || stmt.back() == ':') continue;
            // Skip escaped newlines like \n\t
            if (stmt == "\\n" || stmt == "\\t" || stmt == "\\n\\t") continue;
            result.push_back(parseLine(stmt, lineNum));
        }
        ++lineNum;
    }
    return result;
}

// ═══════════════════════════════════════════════════════════
// Source File Parser — Extract asm blocks from C source
// ═══════════════════════════════════════════════════════════

struct AsmBlock {
    std::string asmString;
    std::vector<std::string> clobbers;
    int lineNumber;  // line in source file where asm starts
};

// Find matching closing paren, handling nested parens, strings, and comments
size_t findMatchingParen(const std::string& src, size_t openPos) {
    int depth = 1;
    bool inString = false;
    for (size_t i = openPos + 1; i < src.size(); ++i) {
        char c = src[i];
        // Skip // line comments (outside strings)
        if (!inString && c == '/' && i + 1 < src.size() && src[i+1] == '/') {
            while (i < src.size() && src[i] != '\n') i++;
            continue;
        }
        // Skip /* block comments */ (outside strings)
        if (!inString && c == '/' && i + 1 < src.size() && src[i+1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i+1] == '/')) i++;
            if (i + 1 < src.size()) i++;
            continue;
        }
        if (c == '"' && (i == 0 || src[i-1] != '\\'))
            inString = !inString;
        if (!inString) {
            if (c == '(') depth++;
            else if (c == ')') {
                depth--;
                if (depth == 0) return i;
            }
        }
    }
    return std::string::npos;
}

// Skip whitespace and C/C++ comments
void skipWhitespaceAndComments(const std::string& src, size_t& i) {
    while (i < src.size()) {
        // Skip whitespace
        if (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r') {
            i++;
            continue;
        }
        // Skip // line comments
        if (i + 1 < src.size() && src[i] == '/' && src[i+1] == '/') {
            i += 2;
            while (i < src.size() && src[i] != '\n') i++;
            if (i < src.size()) i++; // skip the newline
            continue;
        }
        // Skip /* block comments */
        if (i + 1 < src.size() && src[i] == '/' && src[i+1] == '*') {
            i += 2;
            while (i + 1 < src.size() && !(src[i] == '*' && src[i+1] == '/')) i++;
            if (i + 1 < src.size()) i += 2; // skip */
            continue;
        }
        break;
    }
}

// Extract string literal content (handles concatenated strings with comments between them)
std::string extractStringLiteral(const std::string& src, size_t start, size_t& endPos) {
    std::string result;
    size_t i = start;
    while (i < src.size()) {
        // Skip whitespace AND comments between concatenated string literals
        skipWhitespaceAndComments(src, i);
        if (i >= src.size() || src[i] != '"') break;
        // Found a string literal
        i++; // skip opening quote
        while (i < src.size() && src[i] != '"') {
            if (src[i] == '\\' && i + 1 < src.size()) {
                if (src[i+1] == 'n') { result += '\n'; i += 2; }
                else if (src[i+1] == 't') { result += '\t'; i += 2; }
                else if (src[i+1] == '\\') { result += '\\'; i += 2; }
                else if (src[i+1] == '"') { result += '"'; i += 2; }
                else { result += src[i]; i++; }
            } else {
                result += src[i];
                i++;
            }
        }
        if (i < src.size()) i++; // skip closing quote
    }
    endPos = i;
    return result;
}

// Parse clobber section: extract strings between quotes
std::vector<std::string> parseClobbers(const std::string& clobberSection) {
    std::vector<std::string> clobbers;
    size_t i = 0;
    while (i < clobberSection.size()) {
        size_t q1 = clobberSection.find('"', i);
        if (q1 == std::string::npos) break;
        size_t q2 = clobberSection.find('"', q1 + 1);
        if (q2 == std::string::npos) break;
        std::string clob = clobberSection.substr(q1 + 1, q2 - q1 - 1);
        clobbers.push_back(toLower(clob));
        i = q2 + 1;
    }
    return clobbers;
}

int countLineNumber(const std::string& src, size_t pos) {
    int line = 1;
    for (size_t i = 0; i < pos && i < src.size(); i++) {
        if (src[i] == '\n') line++;
    }
    return line;
}

std::vector<AsmBlock> extractAsmBlocks(const std::string& src) {
    std::vector<AsmBlock> blocks;

    // Search for asm( or asm volatile( or __asm__( or __asm__ volatile(
    std::vector<std::string> patterns = {"asm volatile(", "asm(", "__asm__ volatile(", "__asm__(", "asm __volatile__(", "__asm__ __volatile__("};

    size_t searchStart = 0;
    while (searchStart < src.size()) {
        size_t bestPos = std::string::npos;
        std::string bestPattern;

        for (auto& pat : patterns) {
            size_t pos = src.find(pat, searchStart);
            if (pos < bestPos) {
                bestPos = pos;
                bestPattern = pat;
            }
        }

        if (bestPos == std::string::npos) break;

        int lineNum = countLineNumber(src, bestPos);
        size_t openParen = bestPos + bestPattern.size() - 1; // position of '('
        size_t closeParen = findMatchingParen(src, openParen);
        if (closeParen == std::string::npos) {
            searchStart = bestPos + 1;
            continue;
        }

        std::string asmContent = src.substr(openParen + 1, closeParen - openParen - 1);

        // Split by colons to get: asm_string : outputs : inputs : clobbers
        // Handles colons inside string literals and C comments
        std::vector<std::string> sections;
        {
            std::string current;
            bool inString = false;
            int parenDepth = 0;
            for (size_t i = 0; i < asmContent.size(); i++) {
                char c = asmContent[i];
                // Skip // line comments (outside strings)
                if (!inString && c == '/' && i + 1 < asmContent.size() && asmContent[i+1] == '/') {
                    while (i < asmContent.size() && asmContent[i] != '\n') i++;
                    if (i < asmContent.size()) current += '\n';
                    continue;
                }
                // Skip /* block comments */ (outside strings)
                if (!inString && c == '/' && i + 1 < asmContent.size() && asmContent[i+1] == '*') {
                    i += 2;
                    while (i + 1 < asmContent.size() && !(asmContent[i] == '*' && asmContent[i+1] == '/')) i++;
                    if (i + 1 < asmContent.size()) i += 1; // will be incremented by for loop
                    continue;
                }
                if (c == '"' && (i == 0 || asmContent[i-1] != '\\')) {
                    inString = !inString;
                }
                if (!inString) {
                    if (c == '(') parenDepth++;
                    else if (c == ')') parenDepth--;
                    else if (c == ':' && parenDepth == 0) {
                        sections.push_back(current);
                        current.clear();
                        continue;
                    }
                }
                current += c;
            }
            sections.push_back(current);
        }

        AsmBlock block;
        block.lineNumber = lineNum;

        // Section 0 = asm string
        if (!sections.empty()) {
            size_t endPos;
            block.asmString = extractStringLiteral(sections[0], 0, endPos);
        }

        // Section 3 = clobbers (if exists)
        if (sections.size() >= 4) {
            block.clobbers = parseClobbers(sections[3]);
        }

        blocks.push_back(block);
        searchStart = closeParen + 1;
    }

    return blocks;
}

// ═══════════════════════════════════════════════════════════
// Validation Passes
// ═══════════════════════════════════════════════════════════

struct Diagnostic {
    enum Level { Error, Warning, Note };
    Level level;
    std::string message;
    int line;
};

std::vector<Diagnostic> validateBlock(const AsmBlock& block) {
    std::vector<Diagnostic> diags;
    auto instructions = tokenize(block.asmString);
    std::unordered_set<std::string> declaredClobbers(block.clobbers.begin(), block.clobbers.end());

    // Pass 1: Register name validation
    for (auto& instr : instructions) {
        for (auto& op : instr.operands) {
            if (op.kind == AsmOperand::Register && !VALID_REGISTERS.count(op.value)) {
                diags.push_back({Diagnostic::Error,
                    "inline asm: unknown x86-64 register '%" + op.value + "'",
                    block.lineNumber});
            }
        }
    }

    // Pass 2: Clobber declaration check
    for (auto& instr : instructions) {
        if (!WRITE_MNEMONICS.count(instr.mnemonic)) continue;
        if (instr.operands.empty()) continue;
        auto& dst = instr.operands.back();
        if (dst.kind != AsmOperand::Register) continue;
        if (!CALLER_SAVED.count(dst.value)) continue;
        if (!declaredClobbers.count(dst.value)) {
            diags.push_back({Diagnostic::Warning,
                "inline asm: register '%" + dst.value + "' is written but not listed in clobber list",
                block.lineNumber});
        }
    }

    // Pass 3: Stack alignment analysis
    int stackDelta = 0;
    for (auto& instr : instructions) {
        if (instr.mnemonic == "push" || instr.mnemonic == "pushq")
            stackDelta += 8;
        else if (instr.mnemonic == "pop" || instr.mnemonic == "popq")
            stackDelta -= 8;
        else if ((instr.mnemonic == "sub" || instr.mnemonic == "subq") &&
                 !instr.operands.empty() && instr.operands.back().value == "rsp") {
            if (instr.operands.size() >= 2 && instr.operands[0].kind == AsmOperand::Immediate) {
                try {
                    std::string imm = instr.operands[0].value;
                    if (!imm.empty() && imm[0] == '$') imm = imm.substr(1);
                    stackDelta += std::stoi(imm);
                } catch (...) {}
            }
        }
    }
    if (stackDelta != 0) {
        diags.push_back({Diagnostic::Warning,
            "inline asm: stack pointer is not restored; net RSP delta = " +
            std::to_string(stackDelta) + " bytes -- violates 16-byte alignment contract",
            block.lineNumber});
    }

    // Pass 4: Callee-saved register check
    std::unordered_set<std::string> saved, restored;
    for (auto& instr : instructions) {
        if (instr.mnemonic == "push" || instr.mnemonic == "pushq") {
            if (!instr.operands.empty() && CALLEE_SAVED.count(instr.operands[0].value))
                saved.insert(instr.operands[0].value);
        } else if (instr.mnemonic == "pop" || instr.mnemonic == "popq") {
            if (!instr.operands.empty() && CALLEE_SAVED.count(instr.operands[0].value))
                restored.insert(instr.operands[0].value);
        } else if (WRITE_MNEMONICS.count(instr.mnemonic)) {
            if (!instr.operands.empty()) {
                auto& dst = instr.operands.back();
                if (dst.kind == AsmOperand::Register &&
                    CALLEE_SAVED.count(dst.value) &&
                    !saved.count(dst.value) &&
                    !declaredClobbers.count(dst.value)) {
                    diags.push_back({Diagnostic::Error,
                        "inline asm: callee-saved register '%" + dst.value +
                        "' is modified without being pushed/restored -- ABI violation",
                        block.lineNumber});
                }
            }
        }
    }
    for (auto& reg : saved) {
        if (!restored.count(reg)) {
            diags.push_back({Diagnostic::Error,
                "inline asm: callee-saved register '%" + reg +
                "' is pushed but never popped -- stack will be misaligned",
                block.lineNumber});
        }
    }

    return diags;
}

// ═══════════════════════════════════════════════════════════
// Main
// ═══════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "=========================================" << std::endl;
        std::cout << "  Clang Inline Assembly Validator v1.0"    << std::endl;
        std::cout << "  x86-64 / System V AMD64 ABI"            << std::endl;
        std::cout << "=========================================" << std::endl;
        std::cout << std::endl;
        std::cout << "Usage: " << argv[0] << " <file.c> [file2.c ...]" << std::endl;
        std::cout << std::endl;
        std::cout << "Validates inline assembly in C/C++ source files." << std::endl;
        std::cout << "Checks for:" << std::endl;
        std::cout << "  [1] Invalid register names" << std::endl;
        std::cout << "  [2] Missing clobber declarations" << std::endl;
        std::cout << "  [3] Stack pointer misalignment" << std::endl;
        std::cout << "  [4] Callee-saved register corruption" << std::endl;
        return 1;
    }

    int totalErrors = 0;
    int totalWarnings = 0;
    int totalFiles = 0;
    int totalBlocks = 0;

    for (int i = 1; i < argc; i++) {
        std::string filename = argv[i];
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "error: cannot open file '" << filename << "'" << std::endl;
            totalErrors++;
            continue;
        }

        totalFiles++;
        std::string src((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
        file.close();

        auto blocks = extractAsmBlocks(src);
        totalBlocks += blocks.size();

        for (auto& block : blocks) {
            auto diags = validateBlock(block);
            for (auto& d : diags) {
                std::string levelStr;
                if (d.level == Diagnostic::Error) {
                    levelStr = "error";
                    totalErrors++;
                } else if (d.level == Diagnostic::Warning) {
                    levelStr = "warning";
                    totalWarnings++;
                } else {
                    levelStr = "note";
                }
                std::cout << filename << ":" << d.line << ": "
                          << levelStr << ": " << d.message << std::endl;
            }
        }
    }

    // Summary
    std::cout << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "  Validation Summary" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "  Files scanned:    " << totalFiles << std::endl;
    std::cout << "  Asm blocks found: " << totalBlocks << std::endl;
    std::cout << "  Errors:           " << totalErrors << std::endl;
    std::cout << "  Warnings:         " << totalWarnings << std::endl;
    std::cout << "=========================================" << std::endl;

    if (totalErrors > 0) return 1;
    return 0;
}
