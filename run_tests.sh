#!/bin/bash
# run_tests.sh â€” Automated test runner for AsmValidator plugin
# Usage: ./run_tests.sh [path-to-plugin.so]

PLUGIN=${1:-./build/libAsmValidator.so}
FAIL=0
TOTAL=0

echo "============================================="
echo "  AsmValidator Plugin â€” Test Suite"
echo "============================================="
echo "Plugin: $PLUGIN"
echo

if [ ! -f "$PLUGIN" ]; then
    echo "ERROR: Plugin not found at $PLUGIN"
    echo "Build the plugin first:"
    echo "  cmake -B build && cmake --build build"
    exit 1
fi

echo "=== BUGGY TESTS (should produce diagnostics) ==="
echo
for f in tests/buggy_*.c; do
    TOTAL=$((TOTAL+1))
    echo -n "Testing $f ... "
    output=$(clang --target=x86_64 -fplugin=$PLUGIN -fsyntax-only "$f" 2>&1 || true)
    if echo "$output" | grep -qE "(error|warning): inline asm"; then
        echo "PASS (diagnostic emitted)"
    else
        echo "FAIL (no diagnostic)"
        echo "  Output: $output"
        FAIL=$((FAIL+1))
    fi
done

echo
echo "=== CORRECT TESTS (should produce no diagnostics) ==="
echo
for f in tests/correct_*.c; do
    TOTAL=$((TOTAL+1))
    echo -n "Testing $f ... "
    output=$(clang --target=x86_64 -fplugin=$PLUGIN -fsyntax-only "$f" 2>&1 || true)
    if echo "$output" | grep -qE "(error|warning): inline asm"; then
        echo "FAIL (false positive)"
        echo "  $output"
        FAIL=$((FAIL+1))
    else
        echo "PASS (no false positive)"
    fi
done

echo
echo "============================================="
echo "  Results: $((TOTAL - FAIL))/$TOTAL passed"
echo "============================================="
if [ $FAIL -eq 0 ]; then
    echo "All tests passed."
    exit 0
else
    echo "$FAIL test(s) failed."
    exit 1
fi
