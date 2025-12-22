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
I (1019) ENCODE_BENCH: === ESP32-S3 Opus Encode Benchmark ===
I (1019) ENCODE_BENCH: Audio sources:
I (1029) ENCODE_BENCH:   SPEECH (SILK): 38196 bytes, 16 kHz, 1 channel
I (1029) ENCODE_BENCH:   MUSIC (CELT): 497933 bytes, 48 kHz, 2 channels
I (1039) ENCODE_BENCH: Processing: decode packet -> encode packet (timing encode only)
I (1049) ENCODE_BENCH: Test matrix: 40 speech configs + 20 music configs = 60 total
I (1049) ENCODE_BENCH: Free heap: 17070164 bytes
I (1059) ENCODE_BENCH: Free PSRAM: 16774624 bytes
I (1059) ENCODE_BENCH: Free Internal: 295540 bytes

I (1069) ENCODE_BENCH: ========== Iteration 1 ==========

I (1079) ENCODE_BENCH: === SPEECH Encoding Tests (40 configurations) ===

I (1079) ENCODE_BENCH: --- SPEECH: VOIP, complexity=0, bitrate=10000 ---
I (9899) ENCODE_BENCH: Frame (us): min=3733 max=5057 avg=4467.0 sd=448.8 (n=1500)
I (9899) ENCODE_BENCH: Total: 6700 ms (30.0s audio), RTF: 0.223 (4.5x real-time)
I (9899) ENCODE_BENCH: Encoded: 34542 bytes (9211 bps actual, target 10000 bps)

...

I (346419) ENCODE_BENCH: --- SPEECH: AUDIO, complexity=10, bitrate=32000 ---
I (626839) ENCODE_BENCH: Frame (us): min=4316 max=8150 avg=6860.2 sd=603.6 (n=1500)
I (626839) ENCODE_BENCH: Total: 10290 ms (30.0s audio), RTF: 0.343 (2.9x real-time)
I (626839) ENCODE_BENCH: Encoded: 121566 bytes (32418 bps actual, target 32000 bps)

I (626849) ENCODE_BENCH: === MUSIC Encoding Tests (20 configurations) ===

I (626859) ENCODE_BENCH: --- MUSIC: AUDIO, complexity=0, bitrate=64000 ---
I (644049) ENCODE_BENCH: Frame (us): min=5781 max=6811 avg=6556.7 sd=96.0 (n=1500)
I (644049) ENCODE_BENCH: Total: 9835 ms (30.0s audio), RTF: 0.328 (3.1x real-time)
I (644059) ENCODE_BENCH: Encoded: 241654 bytes (64441 bps actual, target 64000 bps)

...

I (1079799) ENCODE_BENCH: --- MUSIC: AUDIO, complexity=10, bitrate=192000 ---
I (1079799) ENCODE_BENCH: Frame (us): min=8537 max=16959 avg=13005.2 sd=1959.1 (n=1500)
I (1079799) ENCODE_BENCH: Total: 19507 ms (30.0s audio), RTF: 0.650 (1.5x real-time)
I (1079799) ENCODE_BENCH: Encoded: 721980 bytes (192528 bps actual, target 192000 bps)

I (1079809) ENCODE_BENCH: === Iteration 1 Summary ===
I (1079819) ENCODE_BENCH: All encodes successful: YES
I (1079819) ENCODE_BENCH: Free heap: 16949064 bytes
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

## Benchmark Results

Results from ESP32-S3 at 240MHz using fixed-point arithmetic. Values show real-time multiplier with RTF in parentheses.

### Speech Encoding (16kHz mono)

#### VOIP Mode

| Complexity | 10 kbit/s | 16 kbit/s | 24 kbit/s | 32 kbit/s |
| ---------- | --------- | --------- | --------- | --------- |
| 0 | 4.5x (0.22) | 3.6x (0.28) | 3.6x (0.28) | 3.6x (0.28) |
| 2 | 2.8x (0.36) | 2.8x (0.36) | 2.8x (0.36) | 2.8x (0.36) |
| 5 | 2.0x (0.50) | 2.0x (0.50) | 2.0x (0.51) | 2.0x (0.51) |
| 8 | 1.4x (0.69) | 1.4x (0.69) | 1.4x (0.69) | 1.4x (0.70) |
| 10 | 1.4x (0.69) | 1.4x (0.69) | 1.4x (0.69) | 1.4x (0.70) |

#### AUDIO Mode

| Complexity | 10 kbit/s | 16 kbit/s | 24 kbit/s | 32 kbit/s |
| ---------- | --------- | --------- | --------- | --------- |
| 0 | 4.5x (0.22) | 3.6x (0.28) | **5.5x (0.18)** | **5.4x (0.18)** |
| 2 | 2.8x (0.36) | 2.8x (0.36) | **4.6x (0.22)** | **4.6x (0.22)** |
| 5 | 2.0x (0.50) | 2.0x (0.50) | **3.0x (0.34)** | **3.0x (0.34)** |
| 8 | 1.4x (0.70) | 1.4x (0.69) | **2.9x (0.34)** | **2.9x (0.34)** |
| 10 | 1.4x (0.70) | 1.4x (0.69) | **2.9x (0.34)** | **2.9x (0.34)** |

**Bold** = CELT/SILK hybrid (faster than SILK at same bitrate)

### Music Encoding (48kHz stereo, AUDIO mode)

| Complexity | 64 kbit/s | 96 kbit/s | 128 kbit/s | 192 kbit/s |
| ---------- | --------- | --------- | ---------- | ---------- |
| 0 | 3.1x (0.33) | 2.9x (0.35) | 2.8x (0.36) | 2.6x (0.39) |
| 2 | 2.6x (0.39) | 2.4x (0.41) | 2.3x (0.43) | 2.2x (0.45) |
| 5 | 1.9x (0.52) | 1.8x (0.55) | 1.8x (0.56) | 1.7x (0.58) |
| 8 | 1.8x (0.55) | 1.7x (0.60) | 1.6x (0.63) | 1.5x (0.65) |
| 10 | 1.8x (0.55) | 1.7x (0.60) | 1.6x (0.63) | 1.5x (0.65) |

### Key Observations

| Finding | Details |
| ------- | ------- |
| Complexity 8 = 10 | No performance difference between complexity 8 and 10 |
| CELT faster than SILK | At 24+ kbit/s in AUDIO mode, encoder switches to CELT (~2x faster) |
| All configs real-time capable | Worst case 1.4x real-time (complexity 8/10 VOIP speech) |
| Bitrate effect (SILK) | Minimal impact on encode time |
| Bitrate effect (CELT) | Higher bitrates slightly slower (more data to process) |

### Summary by Codec

| Codec | Best Case | Worst Case | Notes |
| ----- | --------- | ---------- | ----- |
| SILK (speech) | 4.5x @ c=0 | 1.4x @ c=8+ | Bitrate has little effect on speed |
| CELT (speech) | 5.5x @ c=0 | 2.9x @ c=8+ | ~2x faster than SILK at same complexity |
| CELT (music) | 3.1x @ c=0 | 1.5x @ c=8+ | Stereo 48kHz more demanding than mono 16kHz |

## Performance Characteristics

### Encoder Complexity

Opus complexity ranges from 0 (fastest) to 10 (best quality):

| Complexity | Trade-off |
| ---------- | --------- |
| 0-2 | Fastest encoding, lower quality |
| 5 | Balanced (default in most applications) |
| 8-10 | Best quality, slowest encoding |

Higher complexity uses more CPU cycles for analysis and psychoacoustic modeling. Note that complexity 8 and 10 show identical performance on ESP32-S3.

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
