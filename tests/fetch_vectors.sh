#!/usr/bin/env bash
# Downloads the official Opus test vectors used by the conformance test and installs the
# testvector*.bit / testvector*.dec files into tests/vectors/ (flattened from whatever directory the
# tarball uses). These files are not committed (see tests/.gitignore); run this once before the
# conformance test, locally or in CI.
#
# By default it fetches the RFC 8251 set (the decoder-errata-corrected vectors, correct for a modern
# libopus). Override the source with OPUS_VECTORS_URL, or the destination with the first argument.
#
# Usage: tests/fetch_vectors.sh [dest_dir]
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
DEST="${1:-$HERE/vectors}"
URL="${OPUS_VECTORS_URL:-https://opus-codec.org/static/testvectors/opus_testvectors-rfc8251.tar.gz}"

mkdir -p "$DEST"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "Downloading test vectors:"
echo "  $URL"
if command -v curl >/dev/null 2>&1; then
    curl -fsSL "$URL" -o "$tmp/vectors.tar.gz"
elif command -v wget >/dev/null 2>&1; then
    wget -q "$URL" -O "$tmp/vectors.tar.gz"
else
    echo "ERROR: need curl or wget to download test vectors." >&2
    exit 1
fi

echo "Extracting..."
tar -xzf "$tmp/vectors.tar.gz" -C "$tmp"

# Flatten: the tarball stores the vectors under its own subdirectory.
find "$tmp" -name 'testvector*.bit' -exec cp -f {} "$DEST/" \;
find "$tmp" -name 'testvector*.dec' -exec cp -f {} "$DEST/" \;

count=$(find "$DEST" -name 'testvector*.bit' | wc -l | tr -d ' ')
echo "Installed $count test vectors into $DEST"
if [ "$count" -eq 0 ]; then
    echo "ERROR: no testvector*.bit files found in the archive." >&2
    exit 1
fi
# The RFC 8251 set ships 12 vectors. Fewer (e.g. a truncated download or a changed layout) would
# silently shrink conformance coverage, so flag it. Not fatal: a custom OPUS_VECTORS_URL may differ.
if [ "$count" -lt 12 ]; then
    echo "WARNING: expected at least 12 test vectors, found $count (download or layout may be incomplete)." >&2
fi
