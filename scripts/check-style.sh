#!/bin/bash
#
# Check code style using clang-format
# Usage: ./scripts/check-style.sh [--fix]
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Find clang-format
CLANG_FORMAT="clang-format"
if ! command -v $CLANG_FORMAT &> /dev/null; then
    echo "Error: clang-format not found"
    exit 1
fi

echo "Using clang-format: $($CLANG_FORMAT --version | head -n1)"

# Collect source files
FILES=$(find "$PROJECT_ROOT/src" "$PROJECT_ROOT/shared" "$PROJECT_ROOT/properties" \
    -name "*.c" -o -name "*.h" | \
    grep -v "/build/" | \
    sort)

if [ "$1" == "--fix" ]; then
    echo "Fixing code style..."
    for file in $FILES; do
        $CLANG_FORMAT --style=file -i "$file"
    done
    echo "Done!"
else
    echo "Checking code style..."
    ERRORS=0
    for file in $FILES; do
        if ! $CLANG_FORMAT --style=file --dry-run --Werror "$file" 2>/dev/null; then
            echo "  ✗ $file"
            ERRORS=$((ERRORS + 1))
        fi
    done
    
    if [ $ERRORS -gt 0 ]; then
        echo ""
        echo "Found $ERRORS file(s) with style issues."
        echo "Run './scripts/check-style.sh --fix' to fix them automatically."
        exit 1
    else
        echo "All files pass style check!"
    fi
fi
