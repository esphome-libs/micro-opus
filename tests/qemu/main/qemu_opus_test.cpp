// Copyright 2026 Kevin Ahrendt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/* On-target decode of the RFC 8251 conformance vectors under qemu-system-xtensa
 * (also runnable on real hardware).
 *
 * Built for esp32s3, where CONFIG_OPUS_ENABLE_XTENSA_OPTIMIZATIONS and the float
 * decode path are on by default, so it exercises the Xtensa LX7 assembly
 * (fixed_lx7.h, pitch_lx7.h MAC, mathops_lx7 float2int16, SILK float conversions)
 * that the host x86/arm suite never compiles.
 *
 * opus_compare can't run here: it inflates both signals to float and builds
 * whole-signal spectral buffers (about 30 MB for a 5 MB vector), and this build
 * has no PSRAM. So the device only decodes (one frame buffer, no whole-signal
 * storage) and streams the raw PCM off-chip for the host to compare. Framing
 * matches tests/conformance/decode_vectors.cpp.
 *
 * Two UARTs, assembled by run-image.sh / compare.sh:
 *
 *   UART0, console, captured to the serial log:
 *     ===OPUS_QEMU_START===
 *     vectors=12
 *     @@VEC name=testvector01 ch=2 rate=48000
 *     @@END name=testvector01 ch=2 bytes=1234560 status=ok
 *     (each vector decoded at ch=1 and ch=2)
 *     ===OPUS_QEMU_DONE=== count=24 fail=0
 *
 *   UART1, raw binary: every decode's interleaved int16 PCM, concatenated in the
 *     order the @@END markers appear. run-image.sh routes it to a host file
 *     (-serial file:); compare.sh carves it into per-decode .sw files by the
 *     @@END byte counts and runs opus_compare against the .dec references.
 *
 * Raw PCM on its own UART, instead of base64 over the console, skips the 4/3
 * base64 inflation and encode cost, bypasses the stdio/VFS console stack, and
 * keeps stray ESP-IDF log lines out of the binary stream. Semihosting to a host
 * file would be faster, but ESP-IDF gates its semihosting VFS on an attached JTAG
 * debugger, which QEMU does not present.
 */

#include "driver/uart.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "micro_opus/opus_packet_decoder.h"
#include "qemu_vectors.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr uint32_t kSampleRate = 48000;
// A single Opus packet is at most ~61 KB (code 3, 48 frames of 1275 bytes).
constexpr uint32_t kMaxPacketBytes = 61440;

// Dedicated raw-PCM channel. QEMU wires the UART peripheral straight to its
// chardev backend, so no GPIO routing is needed under emulation.
constexpr uart_port_t kPcmUart = UART_NUM_1;
constexpr int kPcmTxBufBytes = 1 << 14;  // 16 KB driver TX ring; ISR drains to FIFO

uint32_t read_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

// Decode one embedded ".bit" vector at the given channel count, streaming the
// interleaved int16 PCM out the raw UART. Always emits an @@END marker carrying
// the exact byte count written (so the host can carve the concatenated PCM even
// when a decode fails partway). *out_bytes receives that count; returns whether
// the decode completed without error.
bool decode_vector(const qemu_vector_t& vec, int channels, long* out_bytes) {
    std::printf("@@VEC name=%s ch=%d rate=%u\n", vec.name, channels, kSampleRate);
    std::fflush(stdout);

    micro_opus::OpusPacketDecoder decoder(kSampleRate, static_cast<uint8_t>(channels));
    std::vector<uint8_t> out(decoder.get_pcm_format().max_output_bytes());

    long total = 0;
    bool ok = true;
    size_t off = 0;
    while (off < vec.len) {
        if (vec.len - off < 8) {
            std::printf("\n@@FAIL name=%s ch=%d err=truncated_header\n", vec.name, channels);
            ok = false;
            break;
        }
        const uint32_t len = read_be32(vec.data + off);  // [0..3]=len, [4..7]=range (unused)
        off += 8;
        if (len == 0 || len > kMaxPacketBytes || vec.len - off < len) {
            std::printf("\n@@FAIL name=%s ch=%d err=bad_packet_len\n", vec.name, channels);
            ok = false;
            break;
        }

        size_t bytes_written = 0;
        const auto result =
            decoder.decode(vec.data + off, len, out.data(), out.size(), bytes_written);
        off += len;
        if (result != micro_opus::OPUS_PACKET_DECODER_SUCCESS) {
            std::printf("\n@@FAIL name=%s ch=%d err=decode_%d\n", vec.name, channels,
                        static_cast<int>(result));
            ok = false;
            break;
        }
        if (bytes_written > 0) {
            uart_write_bytes(kPcmUart, out.data(), bytes_written);
            total += static_cast<long>(bytes_written);
        }
    }

    *out_bytes = total;
    std::printf("@@END name=%s ch=%d bytes=%ld status=%s\n", vec.name, channels, total,
                ok ? "ok" : "fail");
    std::fflush(stdout);
    return ok;
}

}  // namespace

extern "C" void app_main(void) {
    std::printf("\n===OPUS_QEMU_START===\n");
    std::printf("vectors=%u\n", QEMU_VECTOR_COUNT);
    std::fflush(stdout);

    // Bring up the raw-PCM UART before decoding. uart_write_bytes() blocks on a
    // full TX ring, so the decode loop is paced by how fast QEMU drains the port.
    uart_config_t cfg = {};
    cfg.baud_rate = 115200;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_DISABLE;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;
    // rx buffer must exceed the 128-byte HW FIFO even though we only transmit.
    ESP_ERROR_CHECK(uart_driver_install(kPcmUart, 256, kPcmTxBufBytes, 0, nullptr, 0));
    ESP_ERROR_CHECK(uart_param_config(kPcmUart, &cfg));

    unsigned int count = 0;
    unsigned int fail = 0;
    // Decode at both channel counts, mirroring the host run_vectors.sh, which
    // accepts a match against either the stereo (.dec) or mono (m.dec) reference.
    const int kChannels[] = {1, 2};
    for (unsigned int i = 0; i < QEMU_VECTOR_COUNT; i++) {
        for (int ch : kChannels) {
            long bytes = 0;
            if (!decode_vector(QEMU_VECTORS[i], ch, &bytes)) {
                fail++;
            }
            count++;
        }
    }

    // Drain the raw channel so the host file is complete before the runner sees
    // the sentinel and kills QEMU.
    uart_wait_tx_done(kPcmUart, portMAX_DELAY);
    std::printf("===OPUS_QEMU_DONE=== count=%u fail=%u\n", count, fail);
    std::fflush(stdout);

    // Park so QEMU can be killed cleanly by the runner once it sees the sentinel.
    for (;;) {}
}
