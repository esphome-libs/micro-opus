#!/usr/bin/env bash
# Assemble a 16MB esp32s3 flash image from its three bins and run it under native
# qemu-system-xtensa, capturing the serial output to a log file. Build-system-
# agnostic: the local PlatformIO path (run-qemu.sh) and the CI idf.py path both
# call this with their own bin layout.
#
# The firmware emits on two UARTs: UART0 carries the console/markers (captured to
# <out.log>) and UART1 carries the raw interleaved-int16 PCM, which we route to a
# host file at "<out.log>.pcm". compare.sh consumes both: it carves the PCM file
# into per-decode .sw files using the @@END byte counts in the log and runs
# opus_compare. (Both scripts derive the .pcm path from the log path, so the CLI
# signature is unchanged and CI needs no new argument.)
#
# Usage: run-image.sh <bootloader.bin> <partition-table.bin> <app.bin> <out.log> [--timeout N]
#
# qemu-system-xtensa is found via $QEMU_XTENSA, then ~/.espressif/tools/, then PATH
# (idf.py's export.sh puts the idf_tools-installed qemu on PATH).
set -euo pipefail

if [ "$#" -lt 4 ]; then
    echo "usage: run-image.sh <bootloader.bin> <partition-table.bin> <app.bin> <out.log> [--timeout N]" >&2
    exit 2
fi
BOOTLOADER="$1"
PARTITIONS="$2"
APP="$3"
OUTLOG="$4"
shift 4

TIMEOUT=600
while [ "${1:-}" != "" ]; do
    case "$1" in
        --timeout) shift; TIMEOUT="$1" ;;
        *) echo "unknown arg: $1" >&2; exit 2 ;;
    esac
    shift
done

QEMU="${QEMU_XTENSA:-}"
if [ -z "$QEMU" ]; then
    if [ -x "$HOME/.espressif/tools/qemu-xtensa/bin/qemu-system-xtensa" ]; then
        QEMU="$HOME/.espressif/tools/qemu-xtensa/bin/qemu-system-xtensa"
    elif command -v qemu-system-xtensa >/dev/null 2>&1; then
        QEMU="$(command -v qemu-system-xtensa)"
    else
        echo "qemu-system-xtensa not found. Set QEMU_XTENSA or install the Espressif" >&2
        echo "build from https://github.com/espressif/qemu/releases" >&2
        exit 1
    fi
fi

FLASH="$(mktemp -t opus_qemu_flash).bin"
trap 'rm -f "$FLASH"' EXIT

echo "==> Assembling 16MB flash image (esp32s3: bl@0x0, pt@0x8000, app@0x10000)"
dd if=/dev/zero bs=1048576 count=16 2>/dev/null | LC_ALL=C tr '\000' '\377' > "$FLASH"
dd if="$BOOTLOADER" of="$FLASH" bs=4096 seek=0  conv=notrunc 2>/dev/null
dd if="$PARTITIONS" of="$FLASH" bs=4096 seek=8  conv=notrunc 2>/dev/null
dd if="$APP"        of="$FLASH" bs=4096 seek=16 conv=notrunc 2>/dev/null

PCM="${OUTLOG}.pcm"
echo "==> Running under qemu-system-xtensa (timeout ${TIMEOUT}s)"
echo "    UART0 -> $OUTLOG   UART1 (raw PCM) -> $PCM"
: > "$OUTLOG"
# UART0 (console/markers) muxed onto stdio and captured to the log; UART1 (raw
# PCM) routed straight to the host file by QEMU, which truncates it on open.
"$QEMU" -nographic -no-reboot -machine esp32s3 \
    -serial mon:stdio -serial file:"$PCM" \
    -drive file="$FLASH",if=mtd,format=raw > "$OUTLOG" 2>&1 &
QPID=$!

outcome="timeout"
deadline=$((SECONDS + TIMEOUT))
while [ "$SECONDS" -lt "$deadline" ]; do
    if grep -q "===OPUS_QEMU_DONE===" "$OUTLOG" 2>/dev/null; then outcome="done"; break; fi
    if ! kill -0 "$QPID" 2>/dev/null; then outcome="crashed"; break; fi
    sleep 1
done
kill "$QPID" 2>/dev/null || true
wait "$QPID" 2>/dev/null || true

if [ "$outcome" != "done" ]; then
    echo "==> FAILED: firmware did not reach the done sentinel ($outcome)"
    echo "    last lines:"; tail -15 "$OUTLOG"
    exit 1
fi

echo "==> Decode finished; log -> $OUTLOG, raw PCM -> $PCM ($(wc -c < "$PCM" 2>/dev/null || echo 0) bytes)"
grep -E "^(vectors=|===OPUS_QEMU_DONE===|@@FAIL)" "$OUTLOG" || true
