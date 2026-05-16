#!/bin/bash
# run.sh â€” Run the x86-64 inline assembly validator on all test cases
# Uses the standalone binary (or Clang plugin if available)

set -e

STANDALONE_BIN="./asm_validator"
PLUGIN="./build/libAsmValidator.so"
PASS=0
FAIL=0
TOTAL=0

echo "============================================="
echo "  x86-64 Inline Assembly Validator â€” Test Suite"
echo "============================================="
echo

# --- Determine which validator to use ---
if [ -f "$PLUGIN" ] && command -v clang &>/dev/null; then
    MODE="plugin"
    echo "Mode: Clang plugin ($PLUGIN)"
elif [ -f "$STANDALONE_BIN" ]; then
    MODE="standalone"
    echo "Mode: Standalone binary ($STANDALONE_BIN)"
else
    echo "ERROR: No validator binary found."
    echo "Run ./build.sh first."
    exit 1
fi
echo

# --- Helper: run validator on a single file ---
run_validator() {
    local file="$1"
    if [ "$MODE" = "plugin" ]; then
        clang --target=x86_64 -fplugin="$PLUGIN" -fsyntax-only "$file" 2>&1
    else
        "$STANDALONE_BIN" "$file" 2>&1
    fi
}

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
#  BUGGY TESTS â€” should produce diagnostics
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo "=== BUGGY TESTS (expect errors/warnings) ==="
echo

for f in tests/buggy_*.c; do
    [ -f "$f" ] || continue
    TOTAL=$((TOTAL + 1))
    echo "--- $f ---"
    output=$(run_validator "$f" 2>&1 || true)
    echo "$output"
    echo

    if echo "$output" | grep -qiE "(error|warning):.*inline asm"; then
        echo "  Result: PASS (diagnostic emitted)"
        PASS=$((PASS + 1))
    else
        echo "  Result: FAIL (no diagnostic detected)"
        FAIL=$((FAIL + 1))
    fi
    echo
done

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
#  CORRECT TESTS â€” should produce no diagnostics
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo "=== CORRECT TESTS (expect no diagnostics) ==="
echo

for f in tests/correct_*.c; do
    [ -f "$f" ] || continue
    TOTAL=$((TOTAL + 1))
    echo "--- $f ---"
    output=$(run_validator "$f" 2>&1 || true)
    echo "$output"
    echo

    if echo "$output" | grep -qiE "(error|warning):.*inline asm"; then
        echo "  Result: FAIL (false positive emitted)"
        FAIL=$((FAIL + 1))
    else
        echo "  Result: PASS (no false positives)"
        PASS=$((PASS + 1))
    fi
    echo
done

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
#  SUMMARY
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo "============================================="
echo "  Test Results: $PASS/$TOTAL passed"
echo "============================================="
echo "  Passed:  $PASS"
echo "  Failed:  $FAIL"
echo "  Total:   $TOTAL"
echo "============================================="

if [ "$FAIL" -eq 0 ]; then
    echo "  All tests passed."
    exit 0
else
    echo "  $FAIL test(s) failed."
    exit 1
fi
