# microOpus - Claude Development Guide

ESP-IDF component wrapping the Opus audio codec with PSRAM support and Xtensa DSP optimizations.

## Project Structure

```text
lib/opus/                 # Upstream Opus (git submodule, never modify)
lib/micro-ogg-demuxer/    # Ogg parser (git submodule)
patches/                  # ESP32-specific patches applied at build time
  diffs/                  # .patch files applied to staged opus copy
  celt/xtensa/           # Xtensa assembly optimizations
  silk/xtensa/           # SILK Xtensa optimizations
  stack_alloc.h          # Thread-local pseudostack implementation
  custom_support.h       # PSRAM-aware memory allocation
cmake/                   # Build system modules
  staging.cmake          # Copies opus to build/opus-staged/ and applies patches
include/micro_opus/      # Public API headers
src/                     # OggOpusDecoder implementation
examples/                # ESP-IDF examples
host_examples/           # Host platform tools (opus_to_wav)
```

## Build Commands

### ESP-IDF

```bash
cd examples/decode_benchmark
idf.py set-target esp32s3
idf.py build
idf.py menuconfig  # Component config → Opus Audio Codec
```

### Host (macOS/Linux)

```bash
cd host_examples/opus_to_wav
mkdir build && cd build
cmake .. && make
./opus_to_wav input.opus output.wav

# With sanitizers (recommended for development)
cmake -DENABLE_SANITIZERS=ON .. && make
```

## Key Architecture Decisions

1. **Upstream submodule is never modified** - All customizations via patches
2. **Staging build system** - Copies opus to `build/opus-staged/`, applies patches, builds from there
3. **Thread-safe pseudostack** - Uses C11 `_Thread_local` for per-thread 120KB working memory
4. **PSRAM preference** - Uses `heap_caps_malloc_prefer()` for flexible memory placement

## Configuration (Kconfig)

- **Allocation mode**: THREADSAFE_PSEUDOSTACK (default), NONTHREADSAFE_PSEUDOSTACK, USE_ALLOCA
- **Pseudostack size**: 60-240KB (default 120KB)
- **Memory placement**: Prefer PSRAM/internal, or strict PSRAM-only/internal-only
- **Floating-point**: Enabled by default for ESP32/ESP32-S3

## Common Tasks

### Adding a new patch

1. Create `.patch` file in `patches/diffs/`
2. Add to appropriate list in `cmake/staging.cmake` (core vs xtensa patches)
3. Re-run cmake to trigger re-staging

### Testing changes

```bash
# Run host tests with sanitizers to catch memory issues
cd host_examples/opus_to_wav
cmake -DENABLE_SANITIZERS=ON -B build && cmake --build build
./build/opus_to_wav test.opus /tmp/out.wav

# ESP32 benchmark
cd examples/decode_benchmark
idf.py build flash monitor
```

### Updating opus submodule

```bash
cd lib/opus
git fetch && git checkout <new-version>
cd ../..
git add lib/opus
# Test that patches still apply cleanly
```

## Things to Watch Out For

- **Never modify files in `lib/`** - Use patches instead
- **Pseudostack overflow** - If decode fails, may need to increase `CONFIG_OPUS_PSEUDOSTACK_SIZE`
- **Thread safety** - Default THREADSAFE_PSEUDOSTACK is safe; NONTHREADSAFE is not
- **Xtensa optimizations** - Only for ESP32/ESP32-S3 (LX6/LX7 cores), not RISC-V chips
