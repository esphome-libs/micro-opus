#!/bin/bash

# Run cppcheck whole-program analysis on first-party sources
#
# Complements clang-tidy: cppcheck sees every first-party source in one pass,
# so its unusedFunction check can flag functions with no caller anywhere in
# the project -- something a per-translation-unit tool cannot do. The scan
# includes host_examples/, tests/, and examples/ so public API entry points
# have visible callers; anything unusedFunction still flags is dead beyond
# the API surface.

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

if ! command -v cppcheck &> /dev/null; then
    echo "Error: cppcheck not found (brew install cppcheck / apt-get install cppcheck)"
    exit 1
fi

cd "$ROOT_DIR"

# Suppressions:
#   lib/opus, lib/micro-ogg-demuxer -- vendored upstream / submodule, not linted
#       here (matches clang-tidy exclusions)
#   patches/ -- ESP32-specific patch sources (custom_support.h, Xtensa asm)
#       applied to the staged opus copy at build time, not first-party code
#   useStlAlgorithm -- raw loops are often clearer; stylistic nag
#   functionStatic on the public headers -- OggOpusDecoder / OpusPacketDecoder
#       accessors are instance methods by API design, matching
#       num_channels()/bits_per_sample()
#   missingInclude* -- system, ESP-IDF, and libopus headers are not resolvable
#       here; cppcheck analyzes without them
#   ctuOneDefinitionRuleViolation on examples/ -- decode_benchmark and
#       encode_benchmark are independent firmware programs that are never
#       linked together; cppcheck's whole-program pass wrongly treats their
#       identically-named local structs (AudioConfig) as one link unit
#
# examples/ and tests/qemu/ compile against ESP-IDF headers cppcheck can't see;
# that only shallows the analysis of those files, it doesn't produce false
# positives.
cppcheck \
    --enable=warning,style,unusedFunction \
    --std=c++11 \
    --inline-suppr \
    --quiet \
    --error-exitcode=1 \
    --suppress=missingIncludeSystem \
    --suppress=missingInclude \
    --suppress='*:lib/opus/*' \
    --suppress='*:lib/micro-ogg-demuxer/*' \
    --suppress='*:patches/*' \
    --suppress=useStlAlgorithm \
    --suppress='functionStatic:include/micro_opus/*' \
    --suppress='ctuOneDefinitionRuleViolation:examples/*' \
    -i build \
    -i host_examples/opus_to_wav/build \
    -i host_examples/pseudostack_probe/build \
    -i tests/build \
    -i tests/qemu/.pio \
    -i examples/decode_benchmark/.pio \
    -i examples/encode_benchmark/.pio \
    -I include \
    -I src \
    -I lib/micro-ogg-demuxer/include \
    src \
    host_examples \
    tests \
    examples

echo "cppcheck passed"
