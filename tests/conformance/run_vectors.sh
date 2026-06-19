#!/usr/bin/env bash
# Decodes the official Opus test vectors with decode_vectors (which uses OpusPacketDecoder) and
# checks each decode against its reference .dec file with opus_compare. Each vector is decoded as
# mono and stereo; a decode passes if it matches either the stereo (.dec) or mono (m.dec) reference.
#
# Exits 77 (CTest "skip") when no vectors are present, so the test is a no-op until you run
# tests/fetch_vectors.sh.
#
# Usage: run_vectors.sh <decode_vectors> <opus_compare> <vector_dir> [tmpdir] [rate]
#   tmpdir defaults to the current directory (scratch .sw files land in $PWD if omitted).
#   rate   defaults to 48000 (all RFC 8251 vectors are 48 kHz).
set -uo pipefail

if [ "$#" -lt 3 ]; then
    echo "Usage: $(basename "$0") <decode_vectors> <opus_compare> <vector_dir> [tmpdir] [rate]" >&2
    exit 2
fi

DECODE="$1"
COMPARE="$2"
VEC="$3"
TMP="${4:-.}"
RATE="${5:-48000}"
mkdir -p "$TMP"

shopt -s nullglob
bits=("$VEC"/testvector*.bit)
if [ ${#bits[@]} -eq 0 ]; then
    echo "No test vectors found in '$VEC'."
    echo "Run tests/fetch_vectors.sh to download them. Skipping conformance test."
    exit 77
fi

echo "Found ${#bits[@]} test vectors in '$VEC'"
fail=0
passed=0

for ch in 1 2; do
    copt=""
    [ "$ch" = "2" ] && copt="-s"
    for bit in "${bits[@]}"; do
        base="$(basename "$bit" .bit)"
        ref="$VEC/$base.dec"
        ref_mono="$VEC/${base}m.dec"
        out="$TMP/$base.${ch}ch.sw"

        if ! "$DECODE" "$ch" "$bit" "$out"; then
            echo "FAIL: decode $base (ch=$ch)"
            fail=1
            continue
        fi

        # Pass if the decode matches EITHER the stereo (.dec) or mono (m.dec) reference. Try .dec
        # first and short-circuit on success (opus_compare's FFT is the slow part); only fall back to
        # m.dec if .dec fails. Capture opus_compare's output so the weighted-error detail is shown
        # when a comparison actually fails.
        ok=1
        cmp_log=""
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
        else
            echo "FAIL: $base (ch=$ch) does not match reference"
            [ -n "$cmp_log" ] && echo "$cmp_log"
            fail=1
        fi
    done
done

echo "Conformance: $passed comparisons passed"
exit $fail
