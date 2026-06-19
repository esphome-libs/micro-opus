#!/bin/bash

# Run clang-tidy on production sources (src/, host_examples/) and host test sources
# (tests/unit/, tests/conformance/).
# Requires a compile_commands.json in each build directory (host_examples/opus_to_wav/build
# and tests/build); the tests one is auto-generated via cmake if absent.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${ROOT_DIR}/host_examples/opus_to_wav/build"

# Find clang-tidy
CLANG_TIDY=""
for name in clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16 clang-tidy-15; do
    if command -v "$name" &> /dev/null; then
        CLANG_TIDY="$name"
        break
    fi
done

# Check Homebrew LLVM paths on macOS
if [ -z "$CLANG_TIDY" ]; then
    for path in /opt/homebrew/opt/llvm/bin/clang-tidy /usr/local/opt/llvm/bin/clang-tidy; do
        if [ -x "$path" ]; then
            CLANG_TIDY="$path"
            break
        fi
    done
fi

if [ -z "$CLANG_TIDY" ]; then
    echo "Error: clang-tidy not found"
    exit 1
fi

# Ensure compile_commands.json exists
if [ ! -f "${BUILD_DIR}/compile_commands.json" ]; then
    echo "Generating compile_commands.json..."
    cmake -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${ROOT_DIR}/host_examples/opus_to_wav"
fi

# Find all source files, excluding lib/ and build/ directories
# Note: examples/ and tests/qemu/ excluded as ESP-IDF code can't be checked without ESP-IDF headers.
SOURCES=$(find "$ROOT_DIR/src" "$ROOT_DIR/host_examples" \
    -path '*/build' -prune -o \
    -path '*/lib' -prune -o \
    \( -name '*.cpp' -o -name '*.c' \) -print 2>/dev/null || true)

if [ -z "$SOURCES" ]; then
    echo "No source files found"
    exit 0
fi

# Host test sources use their own compile database (tests/build). They inherit every rule from the
# root .clang-tidy except readability-magic-numbers, which tests/.clang-tidy disables. qemu/ is an
# ESP-IDF app and can't be checked on the host, so it is left out.
TEST_BUILD_DIR="${ROOT_DIR}/tests/build"
TEST_SOURCES=$(find "$ROOT_DIR/tests/unit" "$ROOT_DIR/tests/conformance" \
    -name '*.cpp' -print 2>/dev/null || true)

if [ -n "$TEST_SOURCES" ] && [ ! -f "${TEST_BUILD_DIR}/compile_commands.json" ]; then
    echo "Generating tests/build/compile_commands.json..."
    cmake -B "$TEST_BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "${ROOT_DIR}/tests"
fi

# Parse arguments
FIX_FLAG=""
if [ "$1" = "--fix" ]; then
    FIX_FLAG="--fix"
fi

echo "Running clang-tidy on src/ and host_examples/..."
$CLANG_TIDY -p "$BUILD_DIR" $FIX_FLAG $SOURCES

if [ -n "$TEST_SOURCES" ]; then
    echo "Running clang-tidy on tests/..."
    $CLANG_TIDY -p "$TEST_BUILD_DIR" $FIX_FLAG $TEST_SOURCES
fi
