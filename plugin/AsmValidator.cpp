// AsmValidator.cpp
// Clang plugin that validates x86-64 inline assembly statements.
// Hooks into the AST pipeline via ASTFrontendAction, matches GCCAsmStmt nodes,
// tokenizes the assembly string, and emits diagnostics for:
//   1. Invalid register names
//   2. Missing clobber declarations
//   3. Stack pointer misalignment
//   4. Callee-saved register corruption

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/AST/Stmt.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/SourceLocation.h"
#include "llvm/Config/llvm-config.h"
#include "AsmTokenizer.h"
#include "RegisterTable.h"
#include <unordered_set>
#include <string>
#include <algorithm>

using namespace clang;

// ─────────────────────────────────────────────
// Visitor: walks the AST looking for GCCAsmStmt
// ─────────────────────────────────────────────
class AsmValidatorVisitor
    : public RecursiveASTVisitor<AsmValidatorVisitor> {
public:
    explicit AsmValidatorVisitor(CompilerInstance& CI)
        : CI(CI), Diags(CI.getDiagnostics()),
          SrcMgr(CI.getSourceManager()) {}

    bool VisitGCCAsmStmt(GCCAsmStmt* S) {
        // 1. Extract the raw asm string (API differs between LLVM versions)
        std::string asmStr;
#if LLVM_VERSION_MAJOR >= 22
        asmStr = S->getAsmString();
#else
        if (auto* SL = S->getAsmString()) {
            asmStr = SL->getString().str();
        }
#endif
        if (asmStr.empty()) {
            return true;
        }

        // 2. Extract declared clobbers
        std::unordered_set<std::string> declaredClobbers;
        for (unsigned i = 0; i < S->getNumClobbers(); ++i) {
            std::string clobber = std::string(S->getClobber(i));
            std::transform(clobber.begin(), clobber.end(),
                           clobber.begin(), ::tolower);
            declaredClobbers.insert(clobber);
        }

        // 3. Tokenize the asm string
        auto instructions = AsmTokenizer::tokenize(asmStr);

        // 4. Run validation passes
        validateRegisters(S, instructions);
        validateClobbers(S, instructions, declaredClobbers);
        validateStackAlignment(S, instructions);
        validateCalleeSaved(S, instructions, declaredClobbers);

        return true;
    }

private:
    CompilerInstance& CI;
    DiagnosticsEngine& Diags;
    SourceManager& SrcMgr;

    // ── Pass 1: Validate register names ───────────────────────────────
    void validateRegisters(GCCAsmStmt* S,
                           const std::vector<AsmInstruction>& instrs) {
        for (auto& instr : instrs) {
            for (auto& op : instr.operands) {
                if (op.kind == AsmOperand::Register &&
                    !VALID_REGISTERS.count(op.value)) {
                    unsigned diagID = Diags.getCustomDiagID(
                        DiagnosticsEngine::Error,
                        "inline asm: unknown x86-64 register '%%%0'");
                    Diags.Report(S->getAsmLoc(), diagID)
                        << op.value;
                }
            }
        }
    }

    // ── Pass 2: Validate clobber declarations ─────────────────────────
    void validateClobbers(GCCAsmStmt* S,
                          const std::vector<AsmInstruction>& instrs,
                          const std::unordered_set<std::string>& declared) {
        static const std::unordered_set<std::string> WRITE_MNEMONICS = {
            "mov","movq","movl","movw","movb",
            "add","addq","sub","subq","imul","idiv",
            "xor","xorq","or","and","not","neg",
            "lea","leaq","pop","popq"
        };
        for (auto& instr : instrs) {
            if (!WRITE_MNEMONICS.count(instr.mnemonic)) continue;
            if (instr.operands.empty()) continue;
            auto& dst = instr.operands.back();
            if (dst.kind != AsmOperand::Register) continue;
            if (!CALLER_SAVED.count(dst.value)) continue;
            if (!declared.count(dst.value)) {
                unsigned diagID = Diags.getCustomDiagID(
                    DiagnosticsEngine::Warning,
                    "inline asm: register '%%%0' is written but not listed in clobber list");
                Diags.Report(S->getAsmLoc(), diagID)
                    << dst.value;
            }
        }
    }

    // ── Pass 3: Stack alignment analysis ──────────────────────────────
    void validateStackAlignment(GCCAsmStmt* S,
                                const std::vector<AsmInstruction>& instrs) {
        int stackDelta = 0;
        for (auto& instr : instrs) {
            if (instr.mnemonic == "push" || instr.mnemonic == "pushq")
                stackDelta += 8;
            else if (instr.mnemonic == "pop" || instr.mnemonic == "popq")
                stackDelta -= 8;
            else if ((instr.mnemonic == "sub" || instr.mnemonic == "subq") &&
                     !instr.operands.empty() &&
                     instr.operands.back().value == "rsp") {
                if (instr.operands.size() >= 2 &&
                    instr.operands[0].kind == AsmOperand::Immediate) {
                    try {
                        std::string imm = instr.operands[0].value;
                        if (imm[0] == '$') imm = imm.substr(1);
                        stackDelta += std::stoi(imm);
                    } catch (...) {}
                }
            }
        }
        if (stackDelta != 0) {
            unsigned diagID = Diags.getCustomDiagID(
                DiagnosticsEngine::Warning,
                "inline asm: stack pointer is not restored; net RSP delta = %0 bytes"
                " -- violates 16-byte alignment contract");
            Diags.Report(S->getAsmLoc(), diagID) << stackDelta;
        }
    }

    // ── Pass 4: Callee-saved register modification ────────────────────
    void validateCalleeSaved(GCCAsmStmt* S,
                             const std::vector<AsmInstruction>& instrs,
                             const std::unordered_set<std::string>& declared) {
        static const std::unordered_set<std::string> WRITE_MNEMONICS = {
            "mov","movq","movl","movw","movb",
            "add","addq","sub","subq","xor","xorq",
            "or","and","not","neg","lea","leaq"
        };
        std::unordered_set<std::string> saved, restored;
        for (auto& instr : instrs) {
            if (instr.mnemonic == "push" || instr.mnemonic == "pushq") {
                if (!instr.operands.empty() &&
                    CALLEE_SAVED.count(instr.operands[0].value))
                    saved.insert(instr.operands[0].value);
            } else if (instr.mnemonic == "pop" || instr.mnemonic == "popq") {
                if (!instr.operands.empty() &&
                    CALLEE_SAVED.count(instr.operands[0].value))
                    restored.insert(instr.operands[0].value);
            } else if (WRITE_MNEMONICS.count(instr.mnemonic)) {
                if (!instr.operands.empty()) {
                    auto& dst = instr.operands.back();
                    if (dst.kind == AsmOperand::Register &&
                        CALLEE_SAVED.count(dst.value) &&
                        !saved.count(dst.value) &&
                        !declared.count(dst.value)) {
                        unsigned diagID = Diags.getCustomDiagID(
                            DiagnosticsEngine::Error,
                            "inline asm: callee-saved register '%%%0' is modified"
                            " without being pushed/restored -- ABI violation");
                        Diags.Report(S->getAsmLoc(), diagID) << dst.value;
                    }
                }
            }
        }
        for (auto& reg : saved) {
            if (!restored.count(reg)) {
                unsigned diagID = Diags.getCustomDiagID(
                    DiagnosticsEngine::Error,
                    "inline asm: callee-saved register '%%%0' is pushed"
                    " but never popped -- stack will be misaligned");
                Diags.Report(S->getAsmLoc(), diagID) << reg;
            }
        }
    }
};

// ─────────────────────────────────────────────
// ASTConsumer: drives the visitor
// ─────────────────────────────────────────────
class AsmValidatorConsumer : public ASTConsumer {
    AsmValidatorVisitor Visitor;
public:
    explicit AsmValidatorConsumer(CompilerInstance& CI) : Visitor(CI) {}
    void HandleTranslationUnit(ASTContext& Ctx) override {
        Visitor.TraverseDecl(Ctx.getTranslationUnitDecl());
    }
};

// ─────────────────────────────────────────────
// FrontendAction: entry point
// ─────────────────────────────────────────────
class AsmValidatorAction : public PluginASTAction {
protected:
    std::unique_ptr<ASTConsumer>
    CreateASTConsumer(CompilerInstance& CI, StringRef) override {
        return std::make_unique<AsmValidatorConsumer>(CI);
    }
    bool ParseArgs(const CompilerInstance&,
                   const std::vector<std::string>&) override {
        return true;
    }
    ActionType getActionType() override {
        return AddAfterMainAction; // run after main compilation
    }
};

// ─────────────────────────────────────────────
// Plugin Registration
// ─────────────────────────────────────────────
static FrontendPluginRegistry::Add<AsmValidatorAction>
    X("asm-validator", "Validates x86-64 inline assembly");
