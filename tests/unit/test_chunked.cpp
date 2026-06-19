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

// Stress test for OggOpusDecoder's internal buffering: builds a real multi-page Ogg Opus stream in
// memory (libopus-encoded sine packets), then feeds it to the decoder 64 bytes at a time so packet
// and page boundaries rarely line up with the input chunks. Verifies the decoder reassembles every
// packet without losing, duplicating, or stalling on data. Build with -DENABLE_SANITIZERS=ON to
// catch buffering overruns.

#include "micro_opus/ogg_opus_decoder.h"
#include "ogg_mux.h"
#include "opus.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t SAMPLE_RATE = 48000;
constexpr uint8_t CHANNELS = 2;
constexpr int FRAME_SAMPLES = 960;  // 20 ms @ 48 kHz, per channel
constexpr uint16_t PRE_SKIP = 312;  // Standard Opus pre-skip
constexpr int NUM_PACKETS = 50;     // 1 s of audio
constexpr uint32_t SERIAL = 0xCAFE;
constexpr size_t TINY_CHUNK = 64;  // Deliberately tiny input chunks
constexpr size_t MAX_ITERATIONS = 1000000;

int g_failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::printf("  FAIL: %s\n", message);
        ++g_failures;
    }
}

// Encode NUM_PACKETS stereo sine frames and mux them into a complete Ogg Opus byte stream.
std::vector<uint8_t> build_ogg_stream() {
    int err = 0;
    OpusEncoder* enc = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &err);
    if (enc == nullptr || err != OPUS_OK) {
        std::printf("  FAIL: opus_encoder_create returned %d\n", err);
        return {};
    }
    // Force large (>255 byte) constant-bitrate packets so each Ogg page needs a multi-segment
    // lacing table, exercising the demuxer's segment reassembly across the tiny input chunks.
    opus_encoder_ctl(enc, OPUS_SET_BITRATE(192000));
    opus_encoder_ctl(enc, OPUS_SET_VBR(0));

    std::vector<uint8_t> stream;
    auto append = [&stream](const std::vector<uint8_t>& page) {
        stream.insert(stream.end(), page.begin(), page.end());
    };
    append(micro_opus_test::make_ogg_page(
        micro_opus_test::OGG_FLAG_BOS, 0, SERIAL, 0,
        micro_opus_test::make_opus_head_family0(CHANNELS, PRE_SKIP)));
    append(micro_opus_test::make_ogg_page(0x00, 0, SERIAL, 1, micro_opus_test::make_opus_tags()));

    std::vector<int16_t> pcm(static_cast<size_t>(FRAME_SAMPLES) * CHANNELS);
    double phase = 0.0;
    const double step = 2.0 * 3.14159265358979323846 * 440.0 / SAMPLE_RATE;
    for (int p = 0; p < NUM_PACKETS; ++p) {
        for (int i = 0; i < FRAME_SAMPLES; ++i) {
            const int16_t s = static_cast<int16_t>(std::lround(std::sin(phase) * 10000.0));
            pcm[static_cast<size_t>(i) * CHANNELS + 0] = s;
            pcm[static_cast<size_t>(i) * CHANNELS + 1] = s;
            phase += step;
        }
        std::vector<uint8_t> packet(4000);
        const int bytes = opus_encode(enc, pcm.data(), FRAME_SAMPLES, packet.data(),
                                      static_cast<opus_int32>(packet.size()));
        if (bytes < 0) {
            std::printf("  FAIL: opus_encode returned %d\n", bytes);
            opus_encoder_destroy(enc);
            return {};
        }
        packet.resize(static_cast<size_t>(bytes));

        const uint64_t granule = static_cast<uint64_t>(p + 1) * FRAME_SAMPLES;
        const uint8_t flags = (p == NUM_PACKETS - 1) ? micro_opus_test::OGG_FLAG_EOS : 0x00;
        append(micro_opus_test::make_ogg_page(flags, granule, SERIAL, 2 + p, packet));
    }
    opus_encoder_destroy(enc);
    return stream;
}

}  // namespace

int main() {
    std::printf("OggOpusDecoder chunked-buffering stress test (64-byte input chunks)\n");

    const std::vector<uint8_t> stream = build_ogg_stream();
    check(!stream.empty(), "built a non-empty Ogg stream");
    if (stream.empty()) {
        return 1;
    }
    std::printf("Built %d-packet stream, %zu bytes\n", NUM_PACKETS, stream.size());

    micro_opus::OggOpusDecoder decoder;

    // A sliding window that we top up TINY_CHUNK bytes at a time from the source stream. Sized
    // generously so it can hold a full (multi-segment) page while still being fed 64 bytes at a
    // time. The chunk size, not the window size, is what stresses the buffering.
    std::vector<uint8_t> window(4096);
    size_t window_used = 0;
    size_t src_pos = 0;

    std::vector<int16_t> pcm(static_cast<size_t>(FRAME_SAMPLES) * CHANNELS);

    size_t total_samples = 0;
    size_t total_packets = 0;
    size_t iterations = 0;

    while (src_pos < stream.size() || window_used > 0) {
        if (++iterations > MAX_ITERATIONS) {
            std::printf("  FAIL: iteration cap hit (possible infinite loop)\n");
            return 1;
        }

        // Top up the window with one tiny chunk.
        if (src_pos < stream.size() && window_used < window.size()) {
            const size_t space = window.size() - window_used;
            const size_t to_copy = std::min({space, TINY_CHUNK, stream.size() - src_pos});
            std::memcpy(window.data() + window_used, stream.data() + src_pos, to_copy);
            window_used += to_copy;
            src_pos += to_copy;
        }

        // Drain as many packets as the current window allows.
        bool made_progress = false;
        while (window_used > 0) {
            size_t consumed = 0;
            size_t samples = 0;
            micro_opus::OggOpusResult result =
                decoder.decode(window.data(), window_used, reinterpret_cast<uint8_t*>(pcm.data()),
                               pcm.size() * sizeof(int16_t), consumed, samples);

            if (result != micro_opus::OGG_OPUS_OK) {
                std::printf("  FAIL: decode error %d\n", static_cast<int>(result));
                return 1;
            }

            if (consumed > 0) {
                if (consumed < window_used) {
                    std::memmove(window.data(), window.data() + consumed, window_used - consumed);
                }
                window_used -= consumed;
                made_progress = true;
            }

            if (samples > 0) {
                total_samples += samples;
                ++total_packets;
            } else {
                break;  // Needs more data in the window
            }
        }

        // A full window the decoder cannot advance means a single page is larger than the window.
        if (!made_progress && window_used == window.size()) {
            std::printf("  FAIL: a page exceeds the %zu-byte window\n", window.size());
            return 1;
        }

        // Source exhausted and no progress this round: either we are done (window fully drained) or
        // stuck (bytes remain that the decoder will never consume). Both are terminal. Handling
        // every window_used value here avoids spinning to the iteration cap on a partial window.
        if (src_pos >= stream.size() && !made_progress) {
            if (window_used > 0) {
                std::printf("  FAIL: %zu orphan byte(s) left; decoder consumed nothing\n",
                            window_used);
                return 1;
            }
            break;
        }
    }

    std::printf("Decoded %zu packets, %zu samples/channel\n", total_packets, total_samples);

    // Pre-skip trims PRE_SKIP samples from the front; with a matching final granule the decoder
    // emits (NUM_PACKETS * FRAME_SAMPLES - PRE_SKIP) samples per channel.
    const size_t expected = static_cast<size_t>(NUM_PACKETS) * FRAME_SAMPLES - PRE_SKIP;
    check(total_samples == expected, "sample count == frames*960 - pre_skip");
    if (total_samples != expected) {
        std::printf("    expected %zu, got %zu\n", expected, total_samples);
    }
    check(decoder.get_channels() == CHANNELS, "decoder reports stereo");
    check(decoder.get_sample_rate() == SAMPLE_RATE, "decoder reports 48 kHz");

    if (g_failures == 0) {
        std::printf("PASS: decoder handled 64-byte chunks correctly\n");
        return 0;
    }
    std::printf("FAILED: %d check(s)\n", g_failures);
    return 1;
}
