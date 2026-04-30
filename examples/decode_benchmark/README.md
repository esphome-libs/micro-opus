# Opus Decode Benchmark

Benchmarks Opus decoding performance by decoding two 30-second Ogg Opus clips in a loop, reporting per-frame timing statistics (min/max/avg/stddev). Tests both CELT (music) and SILK (speech) codecs. On the ESP32-S3 it also demonstrates thread-safe concurrent decoding with up to 4 tasks pinned to alternating cores; the plain ESP32 runs a single task only.

Supported targets: **ESP32-S3** (with octal PSRAM) and plain **ESP32** (with quad PSRAM, e.g. WROVER).

## Features

- Two embedded 30-second test audio clips (public domain):
  - **MUSIC (CELT)**: High-bitrate stereo orchestral music (~498KB)
  - **SPEECH (SILK)**: Low-bitrate mono spoken word (~38KB)
- Per-frame timing with statistical analysis
- Interleaved testing of both audio types
- Thread safety demonstration with 1, 2, 3, and 4 concurrent decode tasks (ESP32-S3 only; plain ESP32 runs 1 task)
- Tasks pinned to alternating cores (task 0 → core 0, task 1 → core 1, etc.)
- Pre-configured for maximum performance (240MHz, PSRAM; floating-point on ESP32-S3, fixed-point on plain ESP32)

## Building and Flashing

### Prerequisites

- **PlatformIO** (recommended) OR ESP-IDF v5.0 or later
- ESP32-S3 board with octal PSRAM, or plain ESP32 board with quad PSRAM (e.g. WROVER)

### Option 1: PlatformIO (Recommended)

PlatformIO provides a simplified build process with automatic dependency management.

```bash
cd examples/decode_benchmark

# Build for ESP32-S3 (default/first env)
pio run -e esp32-s3

# Build for plain ESP32 (WROVER)
pio run -e esp32

# Upload and monitor
pio run -e esp32-s3 -t upload -t monitor
```

Chip-specific sdkconfig defaults live in `sdkconfig.defaults.<target>` (e.g. octal PSRAM and S3 cache tuning for ESP32-S3, quad PSRAM for plain ESP32). Common settings live in `sdkconfig.defaults`.

The PlatformIO configuration uses the parent microOpus repository as a component, so no additional setup is required.

### Option 2: Native ESP-IDF

```bash
cd examples/decode_benchmark
idf.py set-target esp32s3   # or: idf.py set-target esp32
idf.py build
idf.py flash monitor
```

`idf.py set-target` automatically picks up `sdkconfig.defaults.<target>` on top of `sdkconfig.defaults`.

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

Each iteration tests both audio types (MUSIC and SPEECH). On ESP32-S3 the test runs at 1, 2, 3, and 4 concurrent tasks; on plain ESP32 it runs a single task only. Sample S3 output:

```text
I (1045) DECODE_BENCH: --- MUSIC (CELT) - 1 concurrent task ---
I (5575) DECODE_BENCH: Task 0 finished (4513 ms)
I (5575) DECODE_BENCH: Task 0: Frame (us): min=2744 max=5324 avg=2998.8 sd=132.0 (n=1501)
I (5575) DECODE_BENCH: Task 0: Total: 4513 ms (30.0s audio), RTF: 0.150 (6.6x real-time), core 0

I (5585) DECODE_BENCH: --- SPEECH (SILK) - 1 concurrent task ---
I (6905) DECODE_BENCH: Task 0 finished (1305 ms)
I (6905) DECODE_BENCH: Task 0: Frame (us): min=771 max=2000 avg=865.6 sd=67.7 (n=1501)
I (6905) DECODE_BENCH: Task 0: Total: 1305 ms (30.0s audio), RTF: 0.044 (23.0x real-time), core 0

...

I (45315) DECODE_BENCH: --- Summary ---
I (45315) DECODE_BENCH: MUSIC (CELT):
I (45315) DECODE_BENCH:   1 task:     4518 ms
I (45325) DECODE_BENCH:   2 tasks:    6370 ms
I (45325) DECODE_BENCH:   3 tasks:   11455 ms
I (45335) DECODE_BENCH:   4 tasks:   13226 ms
I (45335) DECODE_BENCH: SPEECH (SILK):
I (45335) DECODE_BENCH:   1 task:     1311 ms
I (45345) DECODE_BENCH:   2 tasks:    1422 ms
I (45345) DECODE_BENCH:   3 tasks:    2733 ms
I (45355) DECODE_BENCH:   4 tasks:    2875 ms
I (45355) DECODE_BENCH: All decodes successful: YES
I (45365) DECODE_BENCH: Free heap: 17107204 bytes
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
| 1 | 4.5s | 0.150 (6.6x) | Single task on one core |
| 2 | 6.4s | 0.212 (4.7x) | One task per core - nearly 2x throughput |
| 3 | 11.5s | 0.381 (2.6x) | Core 0 has 2 tasks, core 1 has 1 |
| 4 | 13.2s | 0.440 (2.3x) | Two tasks per core |

**SPEECH (SILK - mono 10kbit/s)**:

| Tasks | Wall-clock | Per-task RTF | Notes |
| ----- | ---------- | ------------ | ----- |
| 1 | 1.3s | 0.044 (23.0x) | Single task on one core |
| 2 | 1.4s | 0.047 (21.2x) | One task per core |
| 3 | 2.7s | 0.091 (11.0x) | Core 0 has 2 tasks |
| 4 | 2.9s | 0.096 (10.4x) | Two tasks per core |

With 2 tasks (one per core), total throughput nearly doubles while wall-clock time only increases ~40% for CELT and ~9% for SILK. SILK decoding is significantly faster than CELT due to lower bitrate and mono audio.

### Plain ESP32 Results

The plain ESP32 (LX6 cores, no fast FPU usable from Opus's tight loops, quad PSRAM) is significantly slower than the S3. The benchmark only runs a single concurrent task on this target. Concurrent decoding with the threadsafe pseudostack does not fit, so the default config uses `NONTHREADSAFE_PSEUDOSTACK` and limits the test to 1 task.

Defaults: 240MHz, fixed-point, NONTHREADSAFE_PSEUDOSTACK, pseudostack in PSRAM.

| Codec | Wall-clock | Per-task RTF | Avg frame |
| ----- | ---------- | ------------ | --------- |
| MUSIC (CELT, stereo 128kbit/s) | 22.1s | 0.736 (1.4x) | 14658 us |
| SPEECH (SILK, mono 10kbit/s) | 2.9s | 0.095 (10.5x) | 1893 us |

CELT decoding still keeps up with real-time playback, but with very little headroom (~36% margin per stream). SILK has plenty of headroom for real-time speech use cases.

### Floating-point vs Fixed-point

The ESP32-S3 has a hardware FPU, and Opus can be built in either floating-point (default) or fixed-point mode:

**MUSIC (CELT - stereo 128kbit/s)**:

| Tasks | Floating-point | Fixed-point | Difference |
| ----- | -------------- | ----------- | ---------- |
| 1 | 4.5s (6.6x) | 5.1s (5.8x) | Fixed 14% slower |
| 2 | 6.4s (4.7x) | 6.0s (5.0x) | Fixed 6% faster |
| 4 | 13.2s (2.3x) | 13.0s (2.3x) | Fixed 1% faster |

**SPEECH (SILK - mono 10kbit/s)**:

| Tasks | Floating-point | Fixed-point | Difference |
| ----- | -------------- | ----------- | ---------- |
| 1 | 1.3s (23.0x) | 1.1s (28.1x) | Fixed 18% faster |
| 2 | 1.4s (21.2x) | 1.2s (25.1x) | Fixed 15% faster |
| 4 | 2.9s (10.4x) | 2.4s (12.5x) | Fixed 17% faster |

**Key observations:**

- **SILK is always faster with fixed-point** - The SILK codec's simpler arithmetic benefits from optimized fixed-point code paths.
- **CELT single-task favors floating-point** - Complex FFT transforms benefit from the hardware FPU.
- **CELT multi-task favors fixed-point** - With concurrent tasks, FPU contention becomes a bottleneck. The single hardware FPU must be shared, while fixed-point integer operations run fully parallel across cores.

**Recommendations:**

- Speech-only applications: Use fixed-point for best performance
- Music-only, single stream: Use floating-point (default)
- Multiple concurrent streams: Consider fixed-point to avoid FPU contention

## Thread Safety

On the ESP32-S3 this example demonstrates the thread-safe pseudostack feature. Each FreeRTOS task:

- Creates its own `OggOpusDecoder` instance
- Gets its own 120KB pseudostack via thread-local storage
- Is pinned to a specific core (alternating 0, 1, 0, 1)
- Decodes independently without interference

The concurrent decode shows all tasks running simultaneously with correct results, verifying the thread-local pseudostack works correctly for multi-threaded applications.

**Memory usage with concurrent tasks (ESP32-S3):**

- Each task needs its own 120KB pseudostack
- With 4 concurrent tasks: 480KB total pseudostack memory
- Plus 8KB FreeRTOS stack per task

The plain ESP32 build does not exercise concurrent decoding. It uses `NONTHREADSAFE_PSEUDOSTACK` (a single shared 120KB pseudostack) and limits the benchmark to one task. Concurrent decoding with the threadsafe pseudostack does not fit in available memory and FPU/cache contention from a second task wipes out any throughput gain.

## Configuration

Defaults vary by target:

- **ESP32-S3**: 240MHz, floating-point, THREADSAFE_PSEUDOSTACK, pseudostack in PSRAM, up to 4 concurrent tasks.
- **Plain ESP32**: 240MHz, fixed-point, NONTHREADSAFE_PSEUDOSTACK, pseudostack in PSRAM, 1 concurrent task only. Floating-point is not enabled by default. The LX6's FPU is slower for Opus's tight loops than the optimized fixed-point paths.

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

On ESP32-S3 with 4 concurrent decode tasks, expect ~480KB for pseudostacks plus decoder state. On plain ESP32 the single task uses one 120KB pseudostack.

## Troubleshooting

| Problem | Solution |
| ------- | -------- |
| Watchdog timeout | Disable in menuconfig: Component config → ESP System Settings → Task Watchdog |
| Stack overflow | Increase task stack size |
| Allocation failures | Check PSRAM is enabled, reduce pseudostack size, or set state to prefer PSRAM |
| Concurrent decode fails | Ensure THREADSAFE_PSEUDOSTACK is enabled (default on ESP32-S3; plain ESP32 defaults to NONTHREADSAFE and only supports a single decoder) |

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
