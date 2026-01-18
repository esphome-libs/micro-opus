# ESP32-S3 Opus Decode Benchmark

Benchmarks Opus decoding performance by decoding two 30-second Ogg Opus clips in a loop, reporting per-frame timing statistics (min/max/avg/stddev). Tests both CELT (music) and SILK (speech) codecs. Also demonstrates thread-safe concurrent decoding with up to 4 tasks pinned to alternating cores.

## Features

- Two embedded 30-second test audio clips (public domain):
  - **MUSIC (CELT)**: High-bitrate stereo orchestral music (~498KB)
  - **SPEECH (SILK)**: Low-bitrate mono spoken word (~38KB)
- Per-frame timing with statistical analysis
- Interleaved testing of both audio types
- Thread safety demonstration with 1, 2, 3, and 4 concurrent decode tasks
- Tasks pinned to alternating cores (task 0 → core 0, task 1 → core 1, etc.)
- Pre-configured for maximum performance (240MHz, PSRAM, floating-point)

## Building and Flashing

### Prerequisites

- **PlatformIO** (recommended) OR ESP-IDF v5.0 or later
- ESP32-S3 development board with PSRAM

### Option 1: PlatformIO (Recommended)

PlatformIO provides a simplified build process with automatic dependency management.

```bash
cd examples/decode_benchmark

# Build the project
pio run

# Upload and monitor
pio run -t upload -t monitor
```

The PlatformIO configuration uses the parent microOpus repository as a component, so no additional setup is required.

### Option 2: Native ESP-IDF

```bash
cd examples/decode_benchmark
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

Each iteration tests both audio types (MUSIC and SPEECH) with 1, 2, 3, and 4 concurrent tasks:

```text
I (1042) DECODE_BENCH: --- MUSIC (CELT) - 1 concurrent task ---
I (5992) DECODE_BENCH: Task 0 finished (4934 ms)
I (5992) DECODE_BENCH: Task 0: Frame (us): min=2945 max=5539 avg=3265.8 sd=144.5 (n=1501)
I (6002) DECODE_BENCH: Task 0: Total: 4934 ms (30.0s audio), RTF: 0.164 (6.1x real-time), core 0

I (6002) DECODE_BENCH: --- SPEECH (SILK) - 1 concurrent task ---
I (7332) DECODE_BENCH: Task 0 finished (1313 ms)
I (7332) DECODE_BENCH: Task 0: Frame (us): min=770 max=2211 avg=869.5 sd=76.4 (n=1501)
I (7342) DECODE_BENCH: Task 0: Total: 1313 ms (30.0s audio), RTF: 0.044 (22.8x real-time), core 0

...

I (48092) DECODE_BENCH: --- Summary ---
I (48092) DECODE_BENCH: MUSIC (CELT):
I (48092) DECODE_BENCH:   1 task:     4939 ms
I (48102) DECODE_BENCH:   2 tasks:    6891 ms
I (48102) DECODE_BENCH:   3 tasks:   12244 ms
I (48112) DECODE_BENCH:   4 tasks:   14209 ms
I (48112) DECODE_BENCH: SPEECH (SILK):
I (48122) DECODE_BENCH:   1 task:     1318 ms
I (48122) DECODE_BENCH:   2 tasks:    1435 ms
I (48122) DECODE_BENCH:   3 tasks:    2754 ms
I (48132) DECODE_BENCH:   4 tasks:    2897 ms
I (48132) DECODE_BENCH: All decodes successful: YES
I (48142) DECODE_BENCH: Free heap: 17107204 bytes
```

### Output Fields

- **Frame (us)**: Per-frame decode time statistics (min/max/avg/sd in microseconds, n = frame count)
- **Total**: Wall-clock time to decode all audio
- **RTF**: Real-Time Factor (decode_time / audio_duration). RTF < 1 means faster than real-time
- **Nx real-time**: How many times faster than real-time playback (1/RTF)
- **core N**: Which CPU core the task ran on

### Performance Scaling

The benchmark shows how performance scales with concurrent tasks on the dual-core ESP32-S3:

**MUSIC (CELT - stereo 128kbit/s)**:

| Tasks | Wall-clock | Per-task RTF | Notes |
| ----- | ---------- | ------------ | ----- |
| 1 | 4.9s | 0.16 (6.1x) | Single task on one core |
| 2 | 6.9s | 0.23 (4.4x) | One task per core - nearly 2x throughput |
| 3 | 12.2s | 0.41 (2.5x) | Core 0 has 2 tasks, core 1 has 1 |
| 4 | 14.2s | 0.47 (2.1x) | Two tasks per core |

**SPEECH (SILK - mono 10kbit/s)**:

| Tasks | Wall-clock | Per-task RTF | Notes |
| ----- | ---------- | ------------ | ----- |
| 1 | 1.3s | 0.04 (22.8x) | Single task on one core |
| 2 | 1.4s | 0.05 (21.0x) | One task per core |
| 3 | 2.8s | 0.09 (11.0x) | Core 0 has 2 tasks |
| 4 | 2.9s | 0.10 (10.4x) | Two tasks per core |

With 2 tasks (one per core), total throughput nearly doubles while wall-clock time only increases ~40% for CELT and ~9% for SILK. SILK decoding is significantly faster than CELT due to lower bitrate and mono audio.

### Floating-point vs Fixed-point

The ESP32-S3 has a hardware FPU, and Opus can be built in either floating-point (default) or fixed-point mode:

**MUSIC (CELT - stereo 128kbit/s)**:

| Tasks | Floating-point | Fixed-point | Difference |
| ----- | -------------- | ----------- | ---------- |
| 1 | 4.9s (6.1x) | 5.4s (5.6x) | Fixed 9% slower |
| 2 | 6.9s (4.4x) | 6.4s (4.7x) | Fixed 7% faster |
| 4 | 14.2s (2.1x) | 13.6s (2.2x) | Fixed 4% faster |

**SPEECH (SILK - mono 10kbit/s)**:

| Tasks | Floating-point | Fixed-point | Difference |
| ----- | -------------- | ----------- | ---------- |
| 1 | 1.3s (22.8x) | 1.0s (29.2x) | Fixed 22% faster |
| 2 | 1.4s (21.0x) | 1.2s (26.0x) | Fixed 19% faster |
| 4 | 2.9s (10.4x) | 2.3s (13.0x) | Fixed 20% faster |

**Key observations:**

- **SILK is always faster with fixed-point** - The SILK codec's simpler arithmetic benefits from optimized fixed-point code paths.
- **CELT single-task favors floating-point** - Complex FFT transforms benefit from the hardware FPU.
- **CELT multi-task favors fixed-point** - With concurrent tasks, FPU contention becomes a bottleneck. The single hardware FPU must be shared, while fixed-point integer operations run fully parallel across cores.

**Recommendations:**

- Speech-only applications: Use fixed-point for best performance
- Music-only, single stream: Use floating-point (default)
- Multiple concurrent streams: Consider fixed-point to avoid FPU contention

## Thread Safety

This example demonstrates the thread-safe pseudostack feature. Each FreeRTOS task:

- Creates its own `OggOpusDecoder` instance
- Gets its own 120KB pseudostack via thread-local storage
- Is pinned to a specific core (alternating 0, 1, 0, 1)
- Decodes independently without interference

The concurrent decode shows all tasks running simultaneously with correct results, verifying the thread-local pseudostack works correctly for multi-threaded applications.

**Memory usage with concurrent tasks:**

- Each task needs its own 120KB pseudostack
- With 4 concurrent tasks: 480KB total pseudostack memory
- Plus 8KB FreeRTOS stack per task

## Configuration

The default configuration uses 240MHz, floating-point, THREADSAFE_PSEUDOSTACK, and pseudostack in PSRAM.

To reduce PSRAM usage, set pseudostack to prefer internal RAM via menuconfig (better performance but uses 120KB internal RAM per thread).

## Regenerating Test Audio

The included test audio uses public domain recordings. To regenerate or use different audio:

### Music (CELT)

```bash
# Download source (e.g., from Musopen Collection on Archive.org)
curl -L -o source.flac "https://archive.org/download/MusopenCollectionAsFlac/..."

# Extract 30 seconds and encode to Opus (high bitrate for CELT)
ffmpeg -i source.flac -ss 60 -t 30 -c:a libopus -b:a 128k -vbr on src/test_audio.opus

# Convert to C header
python3 convert_opus.py src/test_audio.opus src/test_audio_music.h \
    --name test_opus_music_data \
    --description "Audio description here"
```

### Speech (SILK)

```bash
# Download speech source (e.g., from LibriVox on Archive.org)
curl -L -o speech.mp3 "https://archive.org/download/art_of_war_librivox/..."

# Extract 30 seconds and encode to Opus (low bitrate mono for SILK)
ffmpeg -i speech.mp3 -t 30 -c:a libopus -b:a 10k -ac 1 -ar 16000 -application voip src/test_audio_speech.opus

# Convert to C header
python3 convert_opus.py src/test_audio_speech.opus src/test_audio_speech.h \
    --name test_opus_speech_data \
    --description "Audio description here"
```

Keep clips ~30 seconds to fit in flash. For ESP-IDF builds, use `main/` instead of `src/`.

**Note**: To ensure SILK codec usage, encode speech at ≤12kbit/s with 16kHz sample rate. Higher bitrates may use Hybrid mode (SILK+CELT).

## Memory Usage

| Type | Size | Notes |
| ---- | ---- | ----- |
| Flash | ~640KB | 100KB code + 498KB music + 38KB speech |
| Task stack | 8KB each | Per FreeRTOS task |
| Decoder state | ~80-150KB | PSRAM preferred |
| Pseudostack | 120KB per thread | PSRAM by default |

With 4 concurrent decode tasks, expect ~480KB for pseudostacks plus decoder state.

## Troubleshooting

| Problem | Solution |
| ------- | -------- |
| Watchdog timeout | Disable in menuconfig: Component config → ESP System Settings → Task Watchdog |
| Stack overflow | Increase task stack size |
| Allocation failures | Check PSRAM is enabled, reduce pseudostack size, or set state to prefer PSRAM |
| Concurrent decode fails | Ensure THREADSAFE_PSEUDOSTACK is enabled (default) |

## Technical Details

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

**Timing**: Uses `esp_timer_get_time()` for microsecond precision. Only measures `decoder.decode()` calls that produce samples.
