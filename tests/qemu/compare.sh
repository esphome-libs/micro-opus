#!/usr/bin/env bash
# Compare the raw PCM the QEMU firmware streamed off UART1 against the RFC
# references with opus_compare. The emulated esp32s3 produced the PCM with the
# Xtensa assembly enabled; this is the host side that checks it still matches.
#
# The firmware writes every decode's interleaved int16 PCM, back-to-back, to a
# single raw file (UART1 -> "<log>.pcm"), and prints one marker per decode on the
# console (UART0 -> the log):
#
#   @@VEC name=testvector01 ch=2 rate=48000
#   @@END name=testvector01 ch=2 bytes=1234560 status=ok
#
# We replay the @@END markers in order and carve that many bytes off the PCM file
# for each decode (a shared read fd advances in lockstep), then run opus_compare.
#
# The accept logic mirrors tests/conformance/run_vectors.sh exactly: each (vector,
# channel-count) decode passes if it matches EITHER the stereo (.dec) or mono
# (m.dec) reference; -s is passed to opus_compare for the 2-channel decode.
#
# Exits 77 (CTest "skip") when the firmware reported zero vectors (i.e. the
# vectors were not fetched before building the firmware).
#
# Usage: compare.sh <qemu.log> <opus_compare> <vector_dir> [tmpdir] [rate]
#   The raw PCM is read from "<qemu.log>.pcm" (written by run-image.sh).
set -uo pipefail

if [ "$#" -lt 3 ]; then
    echo "Usage: $(basename "$0") <qemu.log> <opus_compare> <vector_dir> [tmpdir] [rate]" >&2
    exit 2
fi
LOG="$1"
COMPARE="$2"
VEC="$3"
TMP="${4:-.}"
RATE="${5:-48000}"
PCM="${LOG}.pcm"
mkdir -p "$TMP"

if grep -q "^vectors=0$" "$LOG"; then
    echo "Firmware embedded no vectors (run tests/fetch_vectors.sh before building). Skipping."
    exit 77
fi
if [ ! -f "$PCM" ]; then
    echo "Raw PCM file '$PCM' not found (expected from run-image.sh)." >&2
    exit 1
fi

fail=0
passed=0

# Compare one carved decode against its reference(s).
compare_one() {
    local name="$1" ch="$2" status="$3" out="$4"
    if [ "$status" != "ok" ]; then
        echo "FAIL: on-device decode failed for $name (ch=$ch)"
        fail=1
        return
    fi
    local copt=""
    [ "$ch" = "2" ] && copt="-s"
    local ref="$VEC/$name.dec"
    local ref_mono="$VEC/${name}m.dec"
    local ok=1 cmp_log="" log=""
    if [ -f "$ref" ]; then
        if log=$("$COMPARE" $copt -r "$RATE" "$ref" "$out" 2>&1); then ok=0; else cmp_log="$log"; fi
    fi
    if [ "$ok" -ne 0 ] && [ -f "$ref_mono" ]; then
        if log=$("$COMPARE" $copt -r "$RATE" "$ref_mono" "$out" 2>&1); then
            ok=0
        else
            cmp_log="${cmp_log}${cmp_log:+
}${log}"
        fi
    fi
    if [ "$ok" -eq 0 ]; then
        passed=$((passed + 1))
        echo "PASS: $name (ch=$ch)"
    else
        echo "FAIL: $name (ch=$ch) does not match reference"
        [ -n "$cmp_log" ] && echo "$cmp_log"
        fail=1
    fi
}

# Replay the @@END markers and carve the PCM file. fd 3 is the single forward
# reader over the concatenated PCM; `head -c bytes` advances it in lockstep with
# the markers, so a failed (or zero-byte) decode still keeps the stream aligned.
out="$TMP/decode.sw"
exec 3< "$PCM"
while IFS= read -r line; do
    line=${line%$'\r'}  # ESP-IDF's UART console emits CRLF.
    case "$line" in
        @@END*)
            name=$(printf '%s\n' "$line" | sed -n 's/.*name=\([^ ]*\).*/\1/p')
            ch=$(printf '%s\n' "$line" | sed -n 's/.*ch=\([0-9]*\).*/\1/p')
            bytes=$(printf '%s\n' "$line" | sed -n 's/.*bytes=\([0-9]*\).*/\1/p')
            status=$(printf '%s\n' "$line" | sed -n 's/.*status=\([a-z]*\).*/\1/p')
            : > "$out"
            if [ "${bytes:-0}" -gt 0 ]; then
                head -c "$bytes" <&3 > "$out"
                got=$(wc -c < "$out")
                if [ "$got" -ne "$bytes" ]; then
                    echo "FAIL: PCM stream short for $name (ch=$ch): got $got of $bytes bytes"
                    fail=1
                    continue
                fi
            fi
            compare_one "$name" "$ch" "${status:-ok}" "$out"
            ;;
    esac
done < "$LOG"
exec 3<&-
rm -f "$out"

if [ "$passed" -eq 0 ] && [ "$fail" -eq 0 ]; then
    echo "No decode markers found in '$LOG'."
    exit 1
fi

echo "QEMU conformance: $passed comparisons passed"
exit $fail
