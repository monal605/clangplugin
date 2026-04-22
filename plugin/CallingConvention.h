// CallingConvention.h
#pragma once
//
// System V AMD64 ABI — Register Classification
//
// This header provides documentation and constants for the System V AMD64
// calling convention used on Linux, macOS, and other Unix-like systems.
//
// Caller-saved (volatile) registers:
//   RAX, RCX, RDX, RSI, RDI, R8-R11, XMM0-XMM7
//   These may be freely modified by any function call. The caller must
//   save them before a call if it needs their values afterward.
//
// Callee-saved (non-volatile) registers:
//   RBX, RBP, R12-R15
//   These must be preserved across function calls. If a function (or
//   inline asm block) modifies them, it must save and restore them.
//
// Stack alignment:
//   The stack must be 16-byte aligned before a CALL instruction.
//   After CALL pushes the return address (8 bytes), RSP % 16 == 8
//   at function entry. The prologue must re-align as needed.
//
// Return value:
//   Integer/pointer: RAX (and RDX for 128-bit)
//   Floating point:  XMM0 (and XMM1 for 128-bit)
//
// Function arguments (integer/pointer):
//   RDI, RSI, RDX, RCX, R8, R9 (in order)
//
// Function arguments (floating point):
//   XMM0-XMM7 (in order)
//

#include <string>

namespace abi {

constexpr int STACK_ALIGNMENT = 16;
constexpr int PUSH_POP_SIZE = 8;  // 64-bit mode: push/pop move 8 bytes

} // namespace abi
