# Testing with Sanitizers

Build `opus_to_wav` with AddressSanitizer and UndefinedBehaviorSanitizer to debug a specific file.

## Building with Sanitizers

```bash
cd host_examples/opus_to_wav
cmake -B build -DENABLE_SANITIZERS=ON .
cmake --build build
./build/opus_to_wav input.opus output.wav
```

## What Sanitizers Detect

**AddressSanitizer (ASan)**: Heap/stack buffer overflow, use-after-free, double-free, memory leaks

**UndefinedBehaviorSanitizer (UBSan)**: Integer overflow, division by zero, invalid shifts, misaligned
access

If memory corruption exists, the program aborts with a detailed error showing the exact location and
stack trace.

## Debugging Tips

```bash
# Leak detection / more stack frames
ASAN_OPTIONS=detect_leaks=1:malloc_context_size=30 ./build/opus_to_wav input.opus output.wav
```

Sanitizer builds run 2-3x slower due to instrumentation. This is normal for debug builds.
