# On-target (QEMU) conformance test

Runs the RFC 8251 decoder-conformance vectors on an emulated ESP32-S3 under
`qemu-system-xtensa` to exercise the Xtensa LX7 assembly. The host suite
(`tests/conformance/`) builds with host gcc/clang and never compiles the Xtensa
paths. This firmware is built with xtensa-gcc for `esp32s3`, with
`CONFIG_OPUS_ENABLE_XTENSA_OPTIMIZATIONS` on, so it covers the asm the host suite
cannot:

- `mathops_lx7.c`: `loopnez` float-to-int16 conversion (float build)
- `silk/.../SigProc_FLP_lx7.h`: `round.s` / `float.s` SILK conversions (float build)
- `fixed_lx7.h`: `mulsh` / `clamps` fixed-point multiplies (fixed build)
- `pitch_lx7.h`: `dual_inner_prod` on the Xtensa MAC unit (both; the `FIXED_POINT`
  path only in the fixed build)

There are two build variants, since the ESP32-S3 selects float or fixed at
compile time and the two paths use different assembly:

- **`esp32s3`** (float): the S3 default (`CONFIG_OPUS_FLOATING_POINT=y`).
- **`esp32s3_fixed`** (fixed-point): layers `sdkconfig.fixed.defaults` to set
  `CONFIG_OPUS_FLOATING_POINT=n`. Same target and image, so `run-image.sh` and
  `compare.sh` are unchanged.

Run both to cover both decode paths. CI runs them as a matrix.

## How it works

`opus_compare` can't run on the device: it inflates both signals to float and
builds whole-signal spectral buffers (about 30 MB for a 5 MB vector), and this
build has no PSRAM. So the device decodes and the host compares.

1. On the emulated S3 (`main/qemu_opus_test.cpp`): the 12 `.bit` inputs are baked
   into the app image (`embed_vectors.py` writes `vectors_data.c` at configure
   time; the 16 MB flash and a custom `partitions.csv` make room). Each vector is
   decoded at 1 and 2 channels with `OpusPacketDecoder`, the same framing as
   `tests/conformance/decode_vectors.cpp`. The raw interleaved int16 PCM goes out
   UART1 one frame at a time, so RAM stays flat; the console (UART0) carries only
   per-decode markers (`@@VEC`, `@@END name=... ch=... bytes=... status=ok`).
2. On the host (`compare.sh`): UART1 is routed to a file (`-serial file:`).
   `compare.sh` carves it into per-decode `.sw` files by the `@@END` byte counts,
   then runs a host build of `opus_compare` against the `.dec` references, using
   the same stereo-or-mono accept logic as the host suite.

## Running locally

Needs PlatformIO, Espressif's `qemu-system-xtensa`, and a C compiler. Install
QEMU from <https://github.com/espressif/qemu/releases> (xtensa-softmmu) into
`~/.espressif/tools/qemu-xtensa/`, or point `QEMU_XTENSA` at the binary. The
build must support `-machine esp32s3` (the Espressif fork, not vanilla Homebrew
QEMU). It runs natively on macOS, Apple Silicon and Intel; no Docker.

```bash
tests/fetch_vectors.sh                     # once: vectors must exist before the build
tests/qemu/run-qemu.sh                     # float build, run under QEMU, compare
tests/qemu/run-qemu.sh --env esp32s3_fixed # fixed-point build
tests/qemu/run-qemu.sh --no-build --timeout 900
```

CI runs the same firmware via `idf.py` in the `espressif/idf` container; see the
`cross-qemu` job in `.github/workflows/ci.yml`.

## Files

| File | Purpose |
|---|---|
| `main/qemu_opus_test.cpp` | Firmware: decode each embedded vector, raw PCM out UART1, markers out UART0 |
| `main/embed_vectors.py` | Writes `vectors_data.c` from `tests/vectors/*.bit` at configure time |
| `main/qemu_vectors.h` | Declares the embedded-vector table |
| `partitions.csv` | 12 MB factory app partition to hold the embedded `.bit` inputs |
| `sdkconfig.defaults` | esp32s3, no PSRAM, 16 MB flash, perf optimization (float) |
| `sdkconfig.fixed.defaults` | Fragment layered for the fixed-point variant (`CONFIG_OPUS_FLOATING_POINT=n`) |
| `run-image.sh` | Assemble the flash image, run QEMU, capture UART0 to the log and UART1 to `<log>.pcm` |
| `compare.sh` | Carve `<log>.pcm` by the `@@END` byte counts and run `opus_compare` against the references |
| `run-qemu.sh` | Local driver: PlatformIO build, then `run-image.sh`, then `compare.sh` |

## Transfer channel

The PCM (about 95 MB of raw int16 across the 24 decodes) leaves the chip on its
own UART (UART1, routed to a file with `-serial file:`); the console and markers
stay on UART0. A dedicated binary channel keeps stray ESP-IDF log lines out of
the stream, skips the 4/3 base64 inflation and its encode cost, and runs about
2x faster than base64 over the console VFS. QEMU models both UARTs and routes
each `-serial` to its own backend, so this behaves the same in CI and locally.

Semihosting straight to a host file would be faster still (one host trap per
buffer instead of one MMIO write per byte), but ESP-IDF gates its semihosting VFS
on an attached JTAG debugger (`esp_cpu_dbgr_is_attached()`), which QEMU does not
present, so it is unavailable here. The earlier base64-over-UART0 approach also
worked but moved about 126 MB and was bound by the console UART VFS.
