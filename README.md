# microOpus - Embedded Opus Wrapper

[![CI](https://github.com/esphome-libs/micro-opus/actions/workflows/ci.yml/badge.svg)](https://github.com/esphome-libs/micro-opus/actions/workflows/ci.yml)

An embedded-focused wrapper for the Opus audio codec. Designed as an ESP-IDF component with PSRAM support and Xtensa DSP optimizations for ESP32/ESP32-S3.

## Features

- **Ogg Opus container support**: C++ streaming decoder with zero-copy optimization
- **Xtensa DSP optimizations**: ~17-25% faster decoding on ESP32/ESP32-S3 (see [DSP Instructions](#xtensa-dsp-instructions))
- **PSRAM-aware allocation**: Configurable memory placement with automatic fallback
- **Thread-safe**: Per-thread 120KB pseudostack enables concurrent usage with 5-8KB task stacks
- **Full Opus API**: Encoding, decoding, and multistream functionality

## Supported Targets

| Target | Default | Notes |
| ------ | ------- | ----- |
| ESP32 | Floating-point + Xtensa DSP | Fixed-point optional via menuconfig |
| ESP32-S3 | Floating-point + Xtensa DSP | Fixed-point optional via menuconfig |
| ESP32-C3 | Fixed-point | RISC-V architecture |
| ESP32-C6 | Fixed-point | RISC-V architecture |

## Configuration

```bash
idf.py menuconfig
# Navigate to: Component config → Opus Audio Codec
```

### Memory Allocation

- **Allocation mode**: Thread-safe pseudostack (default), non-threadsafe pseudostack, or alloca
- **Pseudostack size**: 60KB-240KB (default 120KB)

### Memory Placement

Each memory type can be configured independently with four placement options:

- **Prefer PSRAM** (default): Try PSRAM first, fall back to internal RAM
- **Prefer internal RAM**: Better performance, falls back to PSRAM
- **PSRAM only / Internal only**: Strict placement, fails if unavailable

| Memory Type | Size | Notes |
| ----------- | ---- | ----- |
| State | ~30-50KB per instance | Encoder/decoder state |
| Pseudostack | 120KB+ per thread | Working memory, heavily accessed during encode/decode |
| OggOpus buffers | 1-61KB | Ogg demuxer (most packets use zero-copy) |

### Recommended ESP32-S3 Settings

For best performance (if your chip supports these options):

```ini
# PSRAM
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_USE_CAPS_ALLOC=y
CONFIG_SPIRAM_SPEED_80M=y

# Cache
CONFIG_ESP32S3_DATA_CACHE_64KB=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y
CONFIG_ESP32S3_INSTRUCTION_CACHE_32KB=y
```

## Usage Example

### Basic Decoding

```c
#include "opus.h"
#include "esp_log.h"

static const char *TAG = "opus_example";

void opus_decode_example(const uint8_t *opus_data, int opus_bytes) {
    int error;

    // Create decoder (48kHz, stereo)
    OpusDecoder *decoder = opus_decoder_create(48000, 2, &error);
    if (error != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to create decoder: %s", opus_strerror(error));
        return;
    }

    // Decode frame
    int16_t pcm[960 * 2]; // 20ms @ 48kHz, stereo
    int samples = opus_decode(decoder, opus_data, opus_bytes, pcm, 960, 0);

    if (samples < 0) {
        ESP_LOGE(TAG, "Decode error: %s", opus_strerror(samples));
    } else {
        ESP_LOGI(TAG, "Decoded %d samples", samples);
        // Process PCM audio...
    }

    opus_decoder_destroy(decoder);
}
```

### Basic Encoding

```c
#include "opus.h"
#include "esp_log.h"

static const char *TAG = "opus_example";

void opus_encode_example(const int16_t *pcm_data, int frame_size) {
    int error;

    // Create encoder (48kHz, stereo)
    OpusEncoder *encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
    if (error != OPUS_OK) {
        ESP_LOGE(TAG, "Failed to create encoder: %s", opus_strerror(error));
        return;
    }

    // Set bitrate (e.g., 128 kbps)
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(128000));

    // Encode frame
    uint8_t opus_data[4000]; // Maximum packet size
    int bytes = opus_encode(encoder, pcm_data, frame_size, opus_data, sizeof(opus_data));

    if (bytes < 0) {
        ESP_LOGE(TAG, "Encode error: %s", opus_strerror(bytes));
    } else {
        ESP_LOGI(TAG, "Encoded %d bytes", bytes);
        // Transmit or store opus_data...
    }

    opus_encoder_destroy(encoder);
}
```

See the [encode benchmark example](examples/encode_benchmark) for a complete working example along with performance data across different complexity levels and bitrates.

### Ogg Opus Decoding (C++)

The `OggOpusDecoder` is a portable C++ wrapper that works on any platform, not just ESP32. It can be used with the unmodified upstream Opus library and provides efficient streaming decode with zero-copy optimization via [micro-ogg-demuxer](https://github.com/esphome-libs/micro-ogg-demuxer).

```cpp
#include "micro_opus/ogg_opus_decoder.h"

// Create decoder
micro_opus::OggOpusDecoder decoder;

// Allocate output buffer (int16_t for 16-bit PCM samples)
int16_t pcm_buffer[960 * 2];  // 20ms @ 48kHz stereo

// Decode loop
while (have_input_data) {
    size_t bytes_consumed, samples_decoded;

    micro_opus::OggOpusResult result = decoder.decode(
        input_ptr, input_len,
        reinterpret_cast<uint8_t*>(pcm_buffer), sizeof(pcm_buffer),
        bytes_consumed, samples_decoded
    );

    if (result == micro_opus::OGG_OPUS_OK && samples_decoded > 0) {
        // Process decoded PCM samples (int16_t)
        // Send to I2S DAC, save to file, etc.
    }

    // Advance input pointer
    input_ptr += bytes_consumed;
    input_len -= bytes_consumed;
}

// Get stream info
uint32_t sample_rate = decoder.get_sample_rate();
uint8_t channels = decoder.get_channels();
```

See the [decode benchmark example](examples/decode_benchmark) for a complete working example.

## Memory Usage

**PSRAM is strongly recommended.** Without it, multi-threaded usage may exhaust internal RAM.

| Allocation | Size | Notes |
| ---------- | ---- | ----- |
| Pseudostack | 120KB per thread | Working memory during encode/decode |
| Encoder/decoder state | ~30-50KB per instance | Plus shared tables |
| Task stack | 5-8KB | Much smaller than the 40-60KB needed without pseudostack |

### Thread-Safe Architecture

The default thread-safe pseudostack mode provides:

- Per-thread isolation via pthread thread-local storage (TLS)
- Automatic cleanup when threads exit
- C11 `_Thread_local` caching for near-zero overhead (<0.1% vs non-threadsafe mode)

See [examples/decode_benchmark](examples/decode_benchmark) for multi-threaded usage with tasks pinned to different cores.

## Performance

ESP32-S3 @ 240MHz, 48kHz stereo, overall CPU load (dual-core):

- CELT decode: ~8% CPU (floating-point) / ~9% CPU (fixed-point)
- SILK decode: ~2% CPU (floating-point) / ~2% CPU (fixed-point)

Performance varies with bitrate, complexity, sample rate, and cache configuration.

### Fixed-Point vs Floating-Point

**Decoding**: CELT (music) takes roughly 4x more CPU than SILK (speech) to decode. ESP32 and ESP32-S3 default to floating-point, which is ~9% faster for CELT with a single decode task. However, fixed-point is ~22% faster for SILK. With multiple concurrent decode tasks, fixed-point becomes faster for CELT as well due to FPU contention. Consider fixed-point mode for speech-only applications or when running multiple concurrent decoders.

**Encoding**: Fixed-point is strongly recommended for encoding on ESP32-S3. SILK encoding with floating-point is 4-6x slower than fixed-point and fails to achieve real-time at even the lowest complexity settings. CELT encoding is only ~10-40% slower with floating-point. See the [encode benchmark](examples/encode_benchmark) for detailed performance comparisons.

## Xtensa DSP Instructions

ESP32 (LX6) and ESP32-S3 (LX7) use these DSP instructions for ~17-25% faster decoding:

| Instruction | Purpose |
| ----------- | ------- |
| MULSH | High 32 bits of 64-bit multiply |
| CLAMPS | Saturate to N bits |
| NSAU | Count leading zeros |
| MULA.DD.* | Multiply-accumulate |
| ROUND.S | Round float to integer |

Automatically enabled for ESP32 and ESP32-S3 builds.

## License

This project uses a dual-license structure:

- **microOpus wrapper code** (examples, OggOpusDecoder, host tools): [Apache License 2.0](LICENSE)
- **Opus library** (upstream submodule in `lib/opus/`): BSD-style license - see [lib/opus/COPYING](lib/opus/COPYING)
- **Patches** (in `patches/`): BSD-style license to match upstream Opus

The patches maintain BSD licensing for compatibility with the upstream Opus project, allowing potential contribution back to the main repository.

## Links

- [Opus Official Site](https://opus-codec.org/)
- [Opus Documentation](https://opus-codec.org/docs/)
- [ESP-IDF pthread Thread Local Storage](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/pthread.html#thread-local-storage)
