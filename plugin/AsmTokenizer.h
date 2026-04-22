// AsmTokenizer.h
#pragma once
#include <string>
#include <vector>

struct AsmOperand {
    enum Kind { Register, Immediate, Memory, Label, Unknown };
    Kind kind;
    std::string value;  // e.g. "rax", "42", "[rsp+8]"
};

struct AsmInstruction {
    std::string mnemonic;              // e.g. "mov", "push", "add"
    std::vector<AsmOperand> operands;
    int lineOffset;                    // line within the asm block
};

class AsmTokenizer {
public:
    // Parse AT&T-syntax asm string into instructions.
    // Multi-instruction strings are separated by '\n' or ';'
    static std::vector<AsmInstruction> tokenize(const std::string& asmStr);

private:
    static AsmInstruction parseLine(const std::string& line, int lineNum);
    static AsmOperand parseOperand(const std::string& token);
};
