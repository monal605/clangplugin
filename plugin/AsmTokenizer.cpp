// AsmTokenizer.cpp
#include "AsmTokenizer.h"
#include "RegisterTable.h"
#include <sstream>
#include <algorithm>
#include <cctype>

std::vector<AsmInstruction> AsmTokenizer::tokenize(const std::string& asmStr) {
    std::vector<AsmInstruction> result;
    std::istringstream stream(asmStr);
    std::string line;
    int lineNum = 0;
    while (std::getline(stream, line, '\n')) {
        // Also split on ';'
        std::istringstream lineStream(line);
        std::string stmt;
        while (std::getline(lineStream, stmt, ';')) {
            // Strip leading whitespace
            auto start = stmt.find_first_not_of(" \t");
            if (start == std::string::npos) continue;
            stmt = stmt.substr(start);
            // Skip labels and directives
            if (stmt.empty() || stmt[0] == '.' || stmt.back() == ':') continue;
            result.push_back(parseLine(stmt, lineNum));
        }
        ++lineNum;
    }
    return result;
}

AsmInstruction AsmTokenizer::parseLine(const std::string& line, int lineNum) {
    AsmInstruction instr;
    instr.lineOffset = lineNum;
    std::istringstream ss(line);
    ss >> instr.mnemonic;
    // Normalize mnemonic to lowercase
    std::transform(instr.mnemonic.begin(), instr.mnemonic.end(),
                   instr.mnemonic.begin(), ::tolower);
    std::string token;
    while (ss >> token) {
        // Strip trailing commas
        if (!token.empty() && token.back() == ',')
            token.pop_back();
        instr.operands.push_back(parseOperand(token));
    }
    return instr;
}

AsmOperand AsmTokenizer::parseOperand(const std::string& token) {
    AsmOperand op;
    op.value = token;
    if (token.size() > 1 && token[0] == '%') {
        // AT&T register: strip '%' or '%%'
        std::string regName;
        if (token.size() > 2 && token[1] == '%')
            regName = token.substr(2);
        else
            regName = token.substr(1);
        std::transform(regName.begin(), regName.end(), regName.begin(), ::tolower);
        // Skip GCC operand placeholders like %0, %1, %[name]
        if (!regName.empty() && (std::isdigit(regName[0]) || regName[0] == '[')) {
            op.kind = AsmOperand::Unknown;
            return op;
        }
        op.value = regName;
        op.kind = AsmOperand::Register;
    } else if (!token.empty() && (token[0] == '$' || std::isdigit(token[0]) || token[0] == '-')) {
        op.kind = AsmOperand::Immediate;
    } else if (token[0] == '[' || token.find('(') != std::string::npos) {
        op.kind = AsmOperand::Memory;
    } else if (VALID_REGISTERS.count(token)) {
        op.value = token;
        op.kind = AsmOperand::Register;
    } else {
        op.kind = AsmOperand::Unknown;
    }
    return op;
}
