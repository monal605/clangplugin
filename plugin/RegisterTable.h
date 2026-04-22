// RegisterTable.h
#pragma once
#include <unordered_set>
#include <string>

// All valid x86-64 architectural registers
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
    // XMM / YMM / ZMM (partial)
    "xmm0","xmm1","xmm2","xmm3","xmm4","xmm5",
    "xmm6","xmm7","xmm8","xmm9","xmm10","xmm11",
    "xmm12","xmm13","xmm14","xmm15",
    "ymm0","ymm1","ymm2","ymm3","ymm4","ymm5",
    "ymm6","ymm7"
};

// System V AMD64 ABI classification
// Caller-saved (volatile): compiler does NOT preserve across calls
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

// Callee-saved (non-volatile): must be preserved if modified
const std::unordered_set<std::string> CALLEE_SAVED = {
    "rbx","rbp","r12","r13","r14","r15",
    // 32-bit aliases
    "ebx","ebp","r12d","r13d","r14d","r15d"
};
