# Custom ESP-IDF Integration Files

This folder contains custom implementations for ESP-IDF integration with the Opus library, including PSRAM-aware memory allocation, thread-safe pseudostack, and ESP32-S3 Xtensa LX7 optimizations.

> **Note:** These patches are designed and tested against the specific Opus commit in `lib/opus/`. If you update the submodule, patches may fail to apply or produce incorrect results. Test thoroughly after any submodule update.

## Files

### Memory Allocation

#### custom_support.h

Provides PSRAM-aware memory allocation overrides for ESP-IDF builds:

- **`opus_alloc()`**: Allocates encoder/decoder state and tables with configurable memory preference (PSRAM/internal)
- **`opus_alloc_scratch()`**: Allocates pseudostack buffer with configurable memory preference
- **`opus_free()`**: Uses `heap_caps_free()` for proper cleanup
- **`celt_fatal()`**: Error handler for pseudostack overflow conditions

Memory preferences are configured via Kconfig (menuconfig).

#### stack_alloc.h

**Direct replacement** of the original Opus `celt/stack_alloc.h` file. Implements three allocation modes selectable via Kconfig:

1. **THREADSAFE_PSEUDOSTACK** (default): Thread-safe using C11 `_Thread_local` variables
2. **NONTHREADSAFE_PSEUDOSTACK**: Original Opus implementation with global variables
3. **USE_ALLOCA**: Stack-based allocation using `alloca()`

This file patches the upstream `opus/celt/stack_alloc.h` during the build process.

### Thread-Safe Pseudostack (THREADSAFE_PSEUDOSTACK mode)

#### thread_local_stack.h

API declarations for the thread-safe pseudostack:

- **`register_pseudostack_for_cleanup()`**: Registers buffer with pthread TLS for automatic cleanup
- **`_opus_alloc_and_register_pseudostack()`**: Allocates pseudostack and registers for cleanup

#### thread_local_stack.c

Implementation providing:

- **`_Thread_local` variables**: `scratch_ptr` and `global_stack` for zero-overhead access
- **Lazy allocation**: Buffer allocated on first use (same pattern as NONTHREADSAFE mode)
- **pthread TLS cleanup**: Automatic buffer deallocation when threads exit

The implementation uses direct `_Thread_local` variable access for the hot path (PUSH, SAVE_STACK, RESTORE_STACK macros), matching NONTHREADSAFE mode performance. pthread TLS is used solely for registering the cleanup destructor.

### ESP32 Xtensa LX6/LX7 Optimizations

Follows the upstream convention to use the `OPUS_XTENSA_LX7` define, but all the assembly operations used are available on an ESP32 with an LX6 core.

#### celt/ folder

Contains custom CELT codec headers with Xtensa optimizations:

##### celt/mathops.h

Custom version with Xtensa LX7 optimizations for floating point operations.

##### celt/xtensa/ folder

- `fixed_lx7.h` - Optimized fixed point multiplication macros using MULSH instruction
- `mathops_lx7.h` - Optimized floating point operations
- `pitch_lx7.h` - Optimized fixed point dual inner product

#### silk/ folder

Contains custom SILK codec headers with Xtensa optimizations:

##### silk/xtensa/ folder

- `SigProc_FLP_lx7.h` - Optimized fixed point/floating point conversions

## Implementation Strategy

### Memory Allocation (ESP-IDF)

- **Configurable preferences**: State and pseudostack memory location configurable via Kconfig
- **PSRAM support**: Prefer PSRAM, prefer internal, PSRAM only, or internal only
- **Automatic fallback**: Uses `heap_caps_malloc_prefer()` for graceful degradation

### Thread Safety (THREADSAFE_PSEUDOSTACK)

- **Per-thread pseudostack**: Each thread gets its own buffer (120KB default, configurable)
- **Zero overhead**: Direct `_Thread_local` variable access, same as global variables
- **Lazy allocation**: Buffer allocated on first `ALLOC_STACK` call per thread
- **Automatic cleanup**: pthread TLS destructor frees buffer when threads exit
- **Requires**: `CONFIG_FREERTOS_TLSP_DELETION_CALLBACKS` (auto-selected by Kconfig)

### Host Platform Support

For non-ESP-IDF builds (Linux, macOS, Windows):

- Uses standard `malloc()`/`free()` instead of ESP-IDF heap functions
- Thread-safe pseudostack works via standard pthread and C11 `_Thread_local`
- No PSRAM-specific features

## Performance

- **THREADSAFE_PSEUDOSTACK**: Nearly matches NONTHREADSAFE mode performance (<0.1% difference)
- **ESP32 optimizations**: ~17-25% improvement for CELT operations
- **PSRAM allocation**: Conserves internal RAM with minimal performance impact on an ESP32-S3 with octal PSRAM at 80 MHz.
