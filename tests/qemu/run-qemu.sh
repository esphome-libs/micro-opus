#!/usr/bin/env bash
# Build the on-target conformance firmware with PlatformIO, run it under native
# qemu-system-xtensa, and check the decoded PCM against the RFC references with a
# host build of opus_compare. Fully native on the host, no Docker.
#
# This validates the decoder + Xtensa LX7 assembly built with xtensa-gcc, which
# the host suite (host gcc/clang) cannot. CI runs the same firmware via idf.py in
# the espressif/idf image (see .github/workflows/ci.yml); this is the local loop.
#
# Usage:
#   tests/qemu/run-qemu.sh [--no-build] [--timeout SECONDS]
#
# Requirements:
#   - PlatformIO (pio) on PATH.
#   - Espressif's qemu-system-xtensa; see run-image.sh for how it is located.
#   - A C compiler (cc) for the host opus_compare build.
#   - The RFC vectors fetched into tests/vectors/ (tests/fetch_vectors.sh).
set -euo pipefail

PROJ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$PROJ/../.." && pwd)"
ENVNAME="esp32s3"
BUILD=1
# The decode streams ~95 MB of raw PCM over the emulated UART; give it room.
TIMEOUT=900

while [ "${1:-}" != "" ]; do
    case "$1" in
        --no-build) BUILD=0 ;;
        --timeout) shift; TIMEOUT="$1" ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
    shift
done

WORK="$PROJ/.pio/qemu-work"
mkdir -p "$WORK"

if [ "$BUILD" = "1" ]; then
    echo "==> Building conformance firmware (PlatformIO, $ENVNAME)"
    pio run -d "$PROJ" -e "$ENVNAME"
fi

echo "==> Building host opus_compare"
cc -O2 -o "$WORK/opus_compare" "$ROOT/lib/opus/src/opus_compare.c" -lm

BIN_DIR="$PROJ/.pio/build/$ENVNAME"
LOG="$WORK/qemu.log"
"$PROJ/run-image.sh" \
    "$BIN_DIR/bootloader.bin" \
    "$BIN_DIR/partitions.bin" \
    "$BIN_DIR/firmware.bin" \
    "$LOG" \
    --timeout "$TIMEOUT"

echo "==> Comparing decoded PCM against references"
exec "$PROJ/compare.sh" "$LOG" "$WORK/opus_compare" "$ROOT/tests/vectors" "$WORK"
