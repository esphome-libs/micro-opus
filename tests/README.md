# microOpus tests

Host-side test suite for microOpus. It builds the library as a host (fixed-point) static library and
runs two kinds of tests through CTest:

- **`unit/`** holds focused tests of the wrapper and parser code: the `OpusPacketDecoder` and
  `OggOpusDecoder` wrappers and the RFC 7845 header parser.
- **`conformance/`** validates our patched libopus by decoding the official Opus test vectors with
  our decoder and comparing each result against the reference decode with upstream `opus_compare`.

## Running

```bash
# From the repo root
cmake -B tests/build tests
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

Run with sanitizers (recommended during development; CI uses this):

```bash
cmake -B tests/build -DENABLE_SANITIZERS=ON tests
cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

Filter by label:

```bash
ctest --test-dir tests/build -L unit          # fast wrapper/parser tests
ctest --test-dir tests/build -L conformance   # opus_compare vector validation
```

## Conformance test vectors

The conformance test needs the official RFC 8251 test vectors (the updated Opus decoder-conformance
set), which are not committed (they are large and external). Download them once:

```bash
tests/fetch_vectors.sh           # installs testvector*.bit/.dec into tests/vectors/
```

Until they are present, `conformance_vectors` reports **Skipped** rather than failing. The fetch
script pulls the RFC 8251 set by default; override the source with `OPUS_VECTORS_URL`.

## Tests

| Test | Exercises |
|---|---|
| `test_opus_header` | `src/opus_header.cpp`: OpusHead/OpusTags parsing, mapping families, every error path |
| `test_raw_packet` | `OpusPacketDecoder`: encode/decode round-trip, buffer-too-small recovery, PLC, reset |
| `test_silent_channels` | `OggOpusDecoder`: channel mapping family 1 with a silent channel (value 255) |
| `test_chunked` | `OggOpusDecoder`: reassembling a real multi-page stream fed 64 bytes at a time |
| `conformance_vectors` | Patched libopus: official vectors decoded by us vs reference decodes (`opus_compare`) |

### Why the conformance test uses `opus_compare`

`opus_compare` measures the perceptual difference between a decode and a **reference decode** of the
same bitstream, so it needs the `.dec` files shipped with the vectors. The conformance test decodes
each `.bit` vector with our patched libopus and checks the result against its `.dec` reference.
Round-trip encode/decode correctness is covered by `test_raw_packet`.

The host build is fixed-point and the decode still passes against the float-derived references
because `opus_compare` uses a perceptual threshold, not bit-exactness. The vectors are decoder
vectors, so they validate the decode path (notably the always-on `celt_stack_alloc` patch). The
Xtensa DSP optimizations build only for ESP32/ESP32-S3 targets, so they are not exercised by this
host suite.

## Tools

`tools/measure_zerocopy.cpp` is a measurement tool, not a test. It needs the demuxer's debug stats,
so it is opt-in:

```bash
cmake -B tests/build -DBUILD_MEASURE_TOOLS=ON tests
cmake --build tests/build
./tests/build/measure_zerocopy <input.opus>
```
