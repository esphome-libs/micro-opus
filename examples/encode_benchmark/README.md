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
I (9149) ENCODE_BENCH: Frame (us): min=3370 max=4684 avg=4082.5 sd=463.9 (n=1500)
I (9149) ENCODE_BENCH: Total: 6123 ms (30.0s audio), RTF: 0.204 (4.9x real-time)
I (9149) ENCODE_BENCH: Encoded: 34542 bytes (9211 bps actual, target 10000 bps)

...

I (589889) ENCODE_BENCH: --- SPEECH: AUDIO, complexity=10, bitrate=32000 ---
I (602159) ENCODE_BENCH: Frame (us): min=4251 max=8040 avg=6725.8 sd=594.7 (n=1500)
I (602159) ENCODE_BENCH: Total: 10088 ms (30.0s audio), RTF: 0.336 (3.0x real-time)
I (602159) ENCODE_BENCH: Encoded: 121566 bytes (32418 bps actual, target 32000 bps)

I (602169) ENCODE_BENCH: === MUSIC Encoding Tests (20 configurations) ===

I (602179) ENCODE_BENCH: --- MUSIC: AUDIO, complexity=0, bitrate=64000 ---
I (618689) ENCODE_BENCH: Frame (us): min=5530 max=6602 avg=6330.5 sd=98.6 (n=1500)
I (618689) ENCODE_BENCH: Total: 9495 ms (30.0s audio), RTF: 0.317 (3.2x real-time)
I (618699) ENCODE_BENCH: Encoded: 241654 bytes (64441 bps actual, target 64000 bps)

...

I (1041739) ENCODE_BENCH: --- MUSIC: AUDIO, complexity=10, bitrate=192000 ---
I (1041739) ENCODE_BENCH: Frame (us): min=8468 max=16662 avg=12890.2 sd=1985.3 (n=1500)
I (1041749) ENCODE_BENCH: Total: 19335 ms (30.0s audio), RTF: 0.645 (1.6x real-time)
I (1041749) ENCODE_BENCH: Encoded: 721980 bytes (192528 bps actual, target 192000 bps)

I (1041759) ENCODE_BENCH: === Iteration 1 Summary ===
I (1041759) ENCODE_BENCH: All encodes successful: YES
I (1041769) ENCODE_BENCH: Free heap: 16949064 bytes
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

## Benchmark Results (Fixed-Point)

Results from ESP32-S3 at 240MHz using fixed-point arithmetic (`CONFIG_OPUS_FLOATING_POINT=n`). Fixed-point encoding is significantly faster than floating-point on ESP32-S3, especially for SILK at higher complexity levels where floating-point fails to encode in real-time.

Values show real-time multiplier with RTF in parentheses.

### Speech Encoding (16kHz mono)

#### VOIP Mode

| Complexity | 10 kbit/s | 16 kbit/s | 24 kbit/s | 32 kbit/s |
| ---------- | --------- | --------- | --------- | --------- |
| 0 | 4.9x (0.20) | 3.9x (0.26) | 3.8x (0.26) | 3.8x (0.26) |
| 2 | 2.9x (0.34) | 2.9x (0.34) | 2.9x (0.34) | 2.9x (0.35) |
| 5 | 2.1x (0.48) | 2.1x (0.49) | 2.1x (0.49) | 2.0x (0.49) |
| 8 | 1.5x (0.68) | 1.5x (0.68) | 1.5x (0.68) | 1.5x (0.68) |
| 10 | 1.5x (0.68) | 1.5x (0.68) | 1.5x (0.68) | 1.5x (0.68) |

#### AUDIO Mode

| Complexity | 10 kbit/s | 16 kbit/s | 24 kbit/s | 32 kbit/s |
| ---------- | --------- | --------- | --------- | --------- |
| 0 | 5.0x (0.20) | 3.9x (0.26) | **5.5x (0.18)** | **5.4x (0.18)** |
| 2 | 3.0x (0.34) | 2.9x (0.34) | **4.6x (0.22)** | **4.6x (0.22)** |
| 5 | 2.1x (0.48) | 2.1x (0.48) | **3.0x (0.33)** | **3.0x (0.33)** |
| 8 | 1.5x (0.68) | 1.5x (0.67) | **3.0x (0.34)** | **3.0x (0.34)** |
| 10 | 1.5x (0.68) | 1.5x (0.67) | **3.0x (0.34)** | **3.0x (0.34)** |

**Bold** = CELT/SILK hybrid (faster than SILK at same bitrate)

### Music Encoding (48kHz stereo, AUDIO mode)

| Complexity | 64 kbit/s | 96 kbit/s | 128 kbit/s | 192 kbit/s |
| ---------- | --------- | --------- | ---------- | ---------- |
| 0 | 3.2x (0.32) | 3.0x (0.34) | 2.8x (0.35) | 2.6x (0.38) |
| 2 | 2.6x (0.38) | 2.5x (0.41) | 2.4x (0.42) | 2.3x (0.44) |
| 5 | 2.0x (0.51) | 1.9x (0.53) | 1.8x (0.55) | 1.8x (0.57) |
| 8 | 1.8x (0.54) | 1.7x (0.59) | 1.6x (0.62) | 1.6x (0.65) |
| 10 | 1.8x (0.54) | 1.7x (0.59) | 1.6x (0.62) | 1.6x (0.65) |

### Key Observations

| Finding | Details |
| ------- | ------- |
| Complexity 8 = 10 | No performance difference between complexity 8 and 10 |
| CELT faster than SILK | At 24+ kbit/s in AUDIO mode, encoder switches to CELT (~2x faster) |
| All configs real-time capable | Worst case 1.5x real-time (complexity 8/10 VOIP speech) |
| Bitrate effect (SILK) | Minimal impact on encode time |
| Bitrate effect (CELT) | Higher bitrates slightly slower (more data to process) |

### Summary by Codec

| Codec | Best Case | Worst Case | Notes |
| ----- | --------- | ---------- | ----- |
| SILK (speech) | 4.9x @ c=0 | 1.5x @ c=8+ | Bitrate has little effect on speed |
| CELT (speech) | 5.5x @ c=0 | 3.0x @ c=8+ | ~2x faster than SILK at same complexity |
| CELT (music) | 3.2x @ c=0 | 1.6x @ c=8+ | Stereo 48kHz more demanding than mono 16kHz |

## Benchmark Results (Floating-Point)

Results with `CONFIG_OPUS_FLOATING_POINT=y` for comparison. Floating-point is significantly slower for SILK encoding.

### Speech Encoding (16kHz mono) - Floating-Point

| Mode | Complexity | 10 kbit/s | 16 kbit/s | Higher |
| ---- | ---------- | --------- | --------- | ------ |
| VOIP | 0 | 1.1x (0.87) | **0.7x (1.53)** | skipped |
| AUDIO | 0 | 1.1x (0.87) | **0.7x (1.53)** | skipped |

**Bold** = Slower than real-time (RTF > 1.0). Higher complexity levels were not tested because complexity 0 already fails at 16 kbit/s.

### Music Encoding (48kHz stereo, AUDIO mode) - Floating-Point

| Complexity | 64 kbit/s | 96 kbit/s | 128 kbit/s | 192 kbit/s |
| ---------- | --------- | --------- | ---------- | ---------- |
| 0 | 2.9x (0.34) | 2.7x (0.37) | 2.5x (0.40) | 2.2x (0.46) |
| 2 | 2.5x (0.41) | 2.3x (0.44) | 2.1x (0.47) | 1.9x (0.52) |
| 5 | 2.0x (0.49) | 1.9x (0.52) | 1.8x (0.55) | 1.7x (0.61) |
| 8 | 1.4x (0.73) | 1.3x (0.79) | 1.2x (0.84) | 1.1x (0.91) |
| 10 | 1.4x (0.73) | 1.3x (0.79) | 1.2x (0.84) | 1.1x (0.91) |

### Fixed-Point vs Floating-Point Comparison

| Codec | Fixed-Point | Floating-Point | Speedup |
| ----- | ----------- | -------------- | ------- |
| SILK (c=0, 10k) | 4.9x real-time | 1.1x real-time | **4x faster** |
| SILK (c=0, 16k) | 3.9x real-time | 0.7x (fails) | **6x faster** |
| CELT music (c=0, 64k) | 3.2x real-time | 2.9x real-time | 1.1x faster |
| CELT music (c=8, 192k) | 1.6x real-time | 1.1x real-time | 1.5x faster |

**Recommendation**: Use fixed-point (`CONFIG_OPUS_FLOATING_POINT=n`) for encoding on ESP32-S3. SILK encoding with floating-point is not viable for real-time applications.

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
# Fixed-point (currently configured, can change to floating-point)
CONFIG_OPUS_FLOATING_POINT=n
```

## Memory Usage

| Type | Size | Notes |
| ---- | ---- | ----- |
| Flash | ~640KB | 100KB code + 498KB music + 38KB speech |
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
- Reader: Moira Fogarty (October 2006)
- Source: [LibriVox](https://archive.org/details/art_of_war_librivox) on Archive.org
- License: Public Domain
- Format: Ogg Opus 16kHz mono ~10kbit/s (SILK codec)

**Timing**: Uses `esp_timer_get_time()` for microsecond precision. Only measures `opus_encode()` calls.
