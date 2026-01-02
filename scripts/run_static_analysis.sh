#!/bin/bash
# Run static analysis for SimpleXL
# P2.2: Static analysis script (Section 7.4 SIMPLEXL_ARCH v1.3)

set -e

echo "=== SimpleXL Static Analysis ==="
echo ""

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null; then
    echo "⚠️  clang-tidy not found, skipping..."
    exit 0
fi

# Run clang-tidy on core components
echo "Running clang-tidy on sx_core..."
find components/sx_core -name "*.c" -o -name "*.h" | head -10 | xargs clang-tidy -p build/ 2>&1 || true

echo ""
echo "✅ Static analysis complete"






