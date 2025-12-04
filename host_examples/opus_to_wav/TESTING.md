# Testing with Sanitizers

Build and test the host example with memory sanitizers to catch bugs before deploying to ESP32.

## Building with Sanitizers

```bash
cd host_examples/opus_to_wav
mkdir build && cd build
cmake -DENABLE_SANITIZERS=ON ..
cmake --build .
```

Or use the helper script:

```bash
./build_with_sanitizers.sh
```

## What Sanitizers Detect

**AddressSanitizer (ASan)**: Heap/stack buffer overflow, use-after-free, double-free, memory leaks

**UndefinedBehaviorSanitizer (UBSan)**: Integer overflow, division by zero, invalid shifts, misaligned access

## Running Tests

```bash
# Basic conversion
./opus_to_wav input.opus output.wav

# Stress test with small chunks (64 bytes)
./test_chunked input.opus

# Channel mapping validation
./test_silent_channels

# Zero-copy efficiency measurement
./measure_zerocopy input.opus
```

If memory corruption exists, the program aborts with a detailed error showing the exact location and stack trace.

## Performance

Sanitizer builds run 2-3x slower due to instrumentation. This is normal for debug builds.

## Debugging Tips

```bash
# Verbose output
ASAN_OPTIONS=verbosity=1 ./opus_to_wav input.opus output.wav

# Leak detection
ASAN_OPTIONS=detect_leaks=1 ./opus_to_wav input.opus output.wav

# More stack frames
ASAN_OPTIONS=malloc_context_size=30 ./opus_to_wav input.opus output.wav
```
