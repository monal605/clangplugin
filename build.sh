#!/bin/bash
# build.sh â€” Build the x86-64 inline assembly validator
# Supports two modes:
#   1. Clang plugin (requires LLVM/Clang dev libs)
#   2. Standalone binary (fallback, no LLVM dependency)

set -e

echo "============================================="
echo "  Building x86-64 Inline Assembly Validator"
echo "============================================="
echo

STANDALONE_SRC="standalone/AsmValidatorStandalone.cpp"
STANDALONE_BIN="asm_validator"
BUILD_DIR="build"

# --- Try Clang plugin build first ---
build_plugin() {
    echo "[1/2] Attempting Clang plugin build (requires LLVM/Clang dev libs)..."
    if command -v llvm-config &>/dev/null && command -v cmake &>/dev/null; then
        LLVM_DIR=$(llvm-config --cmakedir 2>/dev/null || true)
        if [ -n "$LLVM_DIR" ]; then
            mkdir -p "$BUILD_DIR"
            cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
                  -DLLVM_DIR="$LLVM_DIR" 2>/dev/null && \
            cmake --build "$BUILD_DIR" --target AsmValidator 2>/dev/null
            if [ -f "$BUILD_DIR/libAsmValidator.so" ]; then
                echo "  -> Clang plugin built: $BUILD_DIR/libAsmValidator.so"
                return 0
            fi
        fi
    fi
    echo "  -> Clang plugin build skipped (LLVM dev libs not found)"
    return 1
}

# --- Standalone build (always works with g++ or clang++) ---
build_standalone() {
    echo "[2/2] Building standalone validator..."
    COMPILER=""
    if command -v g++ &>/dev/null; then
        COMPILER="g++"
    elif command -v clang++ &>/dev/null; then
        COMPILER="clang++"
    elif command -v c++ &>/dev/null; then
        COMPILER="c++"
    fi

    if [ -z "$COMPILER" ]; then
        echo "ERROR: No C++ compiler found (g++, clang++, c++)."
        echo "Install one and retry."
        exit 1
    fi

    echo "  Compiler: $COMPILER"
    $COMPILER -std=c++17 -O2 -o "$STANDALONE_BIN" "$STANDALONE_SRC"
    echo "  -> Standalone binary built: ./$STANDALONE_BIN"
}

# Try plugin first, fall back to standalone
build_plugin || true
echo
build_standalone

echo
echo "============================================="
echo "  BUILD SUCCESSFUL"
echo "============================================="
echo "  Standalone: ./$STANDALONE_BIN <file.c> ..."
if [ -f "$BUILD_DIR/libAsmValidator.so" ]; then
    echo "  Plugin:     clang -fplugin=./$BUILD_DIR/libAsmValidator.so -fsyntax-only <file.c>"
fi
echo
