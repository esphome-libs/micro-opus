# ESP32-S3 Opus Encode Benchmark

Benchmarks Opus encoding performance across a matrix of settings using two 30-second audio clips. Tests speech encoding (SILK codec, low bitrates) and music encoding (CELT codec, high bitrates), reporting per-frame timing statistics and actual vs target bitrate.

## Features

- Two embedded 30-second test audio clips (public domain):
  - **SPEECH (SILK)**: 16kHz mono, tests low-bitrate encoding (10-32 kbit/s)
  - **MUSIC (CELT)**: 48kHz stereo, tests high-bitrate encoding (64-192 kbit/s)
- Full test matrix: complexity levels (0, 2, 5, 8, 10) x application modes (VOIP, AUDIO) x bitrates
- Per-frame encoding timing with statistical analysis (min/max/avg/stddev)
- Only encodes are timed (decode time excluded from measurements)
- Actual vs target bitrate comparison
- Auto-skip: stops test series when encoding becomes slower than real-time
- Pre-configured for maximum performance (240MHz, PSRAM, fixed-point)

## Test Matrix

### Speech (40 configurations)

| Mode | Complexity | Bitrates |
| ---- | ---------- | -------- |
| VOIP | 0, 2, 5, 8, 10 | 10k, 16k, 24k, 32k |
| AUDIO | 0, 2, 5, 8, 10 | 10k, 16k, 24k, 32k |

### Music (20 configurations)

| Mode | Complexity | Bitrates |
| ---- | ---------- | -------- |
| AUDIO | 0, 2, 5, 8, 10 | 64k, 96k, 128k, 192k |

## Building and Flashing

### Prerequisites

- **PlatformIO** (recommended) OR ESP-IDF v5.0 or later
- ESP32-S3 development board with PSRAM

### Option 1: PlatformIO (Recommended)

PlatformIO provides a simplified build process with automatic dependency management.

```bash
cd examples/encode_benchmark

# Build the project
pio run

# Upload and monitor
pio run -t upload -t monitor
```

The PlatformIO configuration uses the parent microOpus repository as a component, so no additional setup is required.

### Option 2: Native ESP-IDF

```bash
cd examples/encode_benchmark
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

### Configuration Options

#### PlatformIO

The default configuration is optimized for maximum performance. To customize:

1. Edit `sdkconfig.defaults` to change Opus-specific settings
2. Use `pio run -t menuconfig` for full ESP-IDF configuration

#### Native ESP-IDF

```bash
idf.py menuconfig
```

Navigate to **Component config → Opus Audio Codec** to adjust:

- Memory allocation mode (THREADSAFE_PSEUDOSTACK, NONTHREADSAFE_PSEUDOSTACK, USE_ALLOCA)
- Floating-point vs fixed-point implementation
- Memory preferences (PSRAM vs internal RAM for state/pseudostack)
- Pseudostack size

## Expected Output

Each iteration runs through all encoder configurations for both audio types:

```text
I (1041) ENCODE_BENCH: === SPEECH Encoding Tests (40 configurations) ===

I (1051) ENCODE_BENCH: --- SPEECH: VOIP, complexity=0, bitrate=10000 ---
I (3205) ENCODE_BENCH: Frame (us): min=1250 max=2100 avg=1432.5 sd=85.2 (n=1501)
I (3215) ENCODE_BENCH: Total: 2150 ms (30.0s audio), RTF: 0.072 (13.9x real-time)
I (3225) ENCODE_BENCH: Encoded: 37584 bytes (10028 bps actual, target 10000 bps)

I (3235) ENCODE_BENCH: --- SPEECH: VOIP, complexity=0, bitrate=16000 ---
...

I (35000) ENCODE_BENCH: === MUSIC Encoding Tests (20 configurations) ===

I (35010) ENCODE_BENCH: --- MUSIC: AUDIO, complexity=0, bitrate=64000 ---
I (42500) ENCODE_BENCH: Frame (us): min=4200 max=6800 avg=4950.3 sd=210.5 (n=1501)
I (42510) ENCODE_BENCH: Total: 7430 ms (30.0s audio), RTF: 0.248 (4.0x real-time)
I (42520) ENCODE_BENCH: Encoded: 240125 bytes (64033 bps actual, target 64000 bps)

...

I (90000) ENCODE_BENCH: === Iteration 1 Summary ===
I (90010) ENCODE_BENCH: All encodes successful: YES
I (90020) ENCODE_BENCH: Free heap: 17105432 bytes
```

### Output Fields

- **Frame (us)**: Per-frame encode time statistics (min/max/avg/sd in microseconds, n = frame count)
- **Total**: Wall-clock time spent encoding all frames
- **RTF**: Real-Time Factor (encode_time / audio_duration). RTF < 1 means faster than real-time
- **Nx real-time**: How many times faster than real-time encoding (1/RTF)
- **Encoded**: Compressed size and actual bitrate vs target bitrate

### Auto-Skip Behavior

When encoding becomes slower than real-time (RTF > 1.0), the benchmark skips remaining configurations in that audio type since higher complexity/bitrate settings will be even slower:

```text
W (95000) ENCODE_BENCH: RTF > 1.0, skipping remaining MUSIC tests (higher settings will be slower)
```

## Performance Characteristics

### Encoder Complexity

Opus complexity ranges from 0 (fastest) to 10 (best quality):

| Complexity | Trade-off |
| ---------- | --------- |
| 0-2 | Fastest encoding, lower quality |
| 5 | Balanced (default in most applications) |
| 8-10 | Best quality, slowest encoding |

Higher complexity uses more CPU cycles for analysis and psychoacoustic modeling.

### Application Mode

- **VOIP**: Optimized for speech, prefers SILK codec even at higher bitrates
- **AUDIO**: Optimized for music, prefers CELT codec, uses SILK only at very low bitrates

### Bitrate Impact

Higher bitrates generally:

- Increase encoding time (more data to process)
- Improve audio quality
- Result in larger compressed output

## Configuration

The default configuration uses 240MHz, fixed-point, THREADSAFE_PSEUDOSTACK, and pseudostack in PSRAM.

Key settings in `sdkconfig.defaults`:

```ini
# Larger stack for encoder (uses more working memory than decoder)
CONFIG_ESP_MAIN_TASK_STACK_SIZE=16000

# Fixed-point (currently configured, can change to floating-point)
CONFIG_OPUS_FLOATING_POINT=n
```

Note: The encoder requires more stack and working memory than the decoder. The main task stack is set to 16KB by default.

## Memory Usage

| Type | Size | Notes |
| ---- | ---- | ----- |
| Flash | ~640KB | 100KB code + 498KB music + 38KB speech |
| Main task stack | 16KB | Larger than decoder due to encoder requirements |
| Encoder state | ~24-60KB | Depends on sample rate and channels |
| Pseudostack | 120KB | Shared between encoder and decoder |

## Troubleshooting

| Problem | Solution |
| ------- | -------- |
| Watchdog timeout | Already disabled in default config |
| Stack overflow | Increase `CONFIG_ESP_MAIN_TASK_STACK_SIZE` |
| Allocation failures | Check PSRAM is enabled, reduce pseudostack size, or set state to prefer PSRAM |
| All tests skip | RTF > 1.0 on first test; consider lowering complexity or switching to floating-point |

## Technical Details

**Processing Flow**: For each encoder configuration, the benchmark:

1. Decodes the embedded Opus file packet by packet using `OggOpusDecoder`
2. Accumulates PCM samples until a full 20ms frame is ready
3. Encodes the frame using the raw Opus encoder API (`opus_encode`)
4. Times only the encode step (decode time is excluded)
5. Reports statistics after processing all audio

**Music Audio (CELT)**: Beethoven Symphony No. 3 "Eroica", Op. 55, Movement I, 30s extract.

- Performer: Czech National Symphony Orchestra
- Source: [Musopen Collection](https://archive.org/details/MusopenCollectionAsFlac) on Archive.org
- License: Public Domain
- Format: Ogg Opus 48kHz stereo ~128kbit/s VBR (CELT codec)

**Speech Audio (SILK)**: The Art of War, Chapters 1-2, 30s extract.

- Author: Sun Tzu
- Reader: Moira Fogarty
- Source: [LibriVox](https://archive.org/details/art_of_war_librivox) on Archive.org
- License: Public Domain
- Format: Ogg Opus 16kHz mono ~10kbit/s (SILK codec)

**Timing**: Uses `esp_timer_get_time()` for microsecond precision. Only measures `opus_encode()` calls.
