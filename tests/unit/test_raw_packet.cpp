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

// Round-trip test for OpusPacketDecoder: encode sine-wave frames with libopus, then decode the
// raw packets (no Ogg container) and verify output sizing, buffer-too-small recovery, packet-loss
// concealment, and reset(). Build with -DENABLE_SANITIZERS=ON to catch memory errors.

#include "micro_opus/opus_packet_decoder.h"
#include "opus.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr uint32_t SAMPLE_RATE = 48000;
constexpr uint8_t CHANNELS = 2;
constexpr int FRAME_SAMPLES = 960;  // 20 ms at 48 kHz, per channel
constexpr size_t FRAME_BYTES = static_cast<size_t>(FRAME_SAMPLES) * CHANNELS * sizeof(int16_t);

int g_failures = 0;

void check(bool condition, const char* message) {
    if (!condition) {
        std::printf("  FAIL: %s\n", message);
        ++g_failures;
    }
}

// Fill one interleaved stereo frame with a sine wave so the encoder has real signal to work with.
void fill_sine_frame(std::vector<int16_t>& pcm, double& phase) {
    pcm.resize(static_cast<size_t>(FRAME_SAMPLES) * CHANNELS);
    const double step = 2.0 * 3.14159265358979323846 * 440.0 / SAMPLE_RATE;
    for (int i = 0; i < FRAME_SAMPLES; ++i) {
        int16_t sample = static_cast<int16_t>(std::lround(std::sin(phase) * 12000.0));
        pcm[static_cast<size_t>(i) * CHANNELS + 0] = sample;
        pcm[static_cast<size_t>(i) * CHANNELS + 1] = sample;
        phase += step;
    }
}

}  // namespace

int main() {
    std::printf("OpusPacketDecoder round-trip test\n");

    // --- Encode a handful of frames into raw Opus packets ---
    int enc_error = 0;
    OpusEncoder* encoder =
        opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &enc_error);
    if (encoder == nullptr || enc_error != OPUS_OK) {
        std::printf("  FAIL: could not create encoder (%d)\n", enc_error);
        return 1;
    }

    constexpr int NUM_FRAMES = 10;
    std::vector<std::vector<uint8_t>> packets;
    std::vector<int16_t> pcm;
    double phase = 0.0;
    for (int f = 0; f < NUM_FRAMES; ++f) {
        fill_sine_frame(pcm, phase);
        std::vector<uint8_t> packet(4000);
        int bytes = opus_encode(encoder, pcm.data(), FRAME_SAMPLES, packet.data(),
                                static_cast<opus_int32>(packet.size()));
        if (bytes < 0) {
            std::printf("  FAIL: opus_encode returned %d\n", bytes);
            opus_encoder_destroy(encoder);
            return 1;
        }
        packet.resize(static_cast<size_t>(bytes));
        packets.push_back(std::move(packet));
    }
    opus_encoder_destroy(encoder);

    // --- Decode the packets and verify ---
    micro_opus::OpusPacketDecoder decoder(SAMPLE_RATE, CHANNELS);

    // PcmFormat is valid immediately (pre-framed: format comes from the constructor).
    const auto& fmt = decoder.get_pcm_format();
    check(fmt.is_valid(), "format valid before first decode");
    check(fmt.sample_rate() == SAMPLE_RATE, "format sample_rate");
    check(fmt.num_channels() == CHANNELS, "format num_channels");
    check(fmt.bytes_per_sample() == 2, "format bytes_per_sample");
    // max_output_bytes is the 120 ms upper bound: 48000/1000*120 = 5760 samples/ch.
    check(fmt.max_output_bytes() == 5760U * CHANNELS * 2U, "format max_output_bytes");

    // int16_t output buffer: naturally aligned for the decoder's 16-bit PCM writes. The API stays
    // byte-oriented, so cast to uint8_t* and pass the size in bytes at each call site.
    std::vector<int16_t> out(fmt.max_output_bytes() / sizeof(int16_t));
    for (const auto& packet : packets) {
        size_t bytes_written = 0;
        auto result =
            decoder.decode(packet.data(), packet.size(), reinterpret_cast<uint8_t*>(out.data()),
                           out.size() * sizeof(int16_t), bytes_written);
        check(result == micro_opus::OPUS_PACKET_DECODER_SUCCESS, "decode succeeds");
        check(bytes_written == FRAME_BYTES, "decode bytes_written == one 20 ms stereo frame");
    }

    // --- Recoverable OUTPUT_BUFFER_TOO_SMALL: tiny buffer, then grow and retry ---
    {
        size_t bytes_written = 12345;
        std::vector<int16_t> tiny(8);  // 16 bytes: far too small for one frame
        auto result = decoder.decode(packets[0].data(), packets[0].size(),
                                     reinterpret_cast<uint8_t*>(tiny.data()),
                                     tiny.size() * sizeof(int16_t), bytes_written);
        check(result == micro_opus::OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL,
              "small buffer => OUTPUT_BUFFER_TOO_SMALL");
        check(bytes_written == 0, "failed decode clears bytes_written");
        size_t needed = decoder.get_required_output_bytes();
        check(needed == FRAME_BYTES, "get_required_output_bytes reports exact size");

        std::vector<int16_t> grown(needed / sizeof(int16_t));
        result = decoder.decode(packets[0].data(), packets[0].size(),
                                reinterpret_cast<uint8_t*>(grown.data()),
                                grown.size() * sizeof(int16_t), bytes_written);
        check(result == micro_opus::OPUS_PACKET_DECODER_SUCCESS, "retry after grow succeeds");
        check(bytes_written == FRAME_BYTES, "retry produces full frame");
    }

    // --- Packet-loss concealment ---
    {
        size_t bytes_written = 0;
        auto result =
            decoder.conceal_loss(reinterpret_cast<uint8_t*>(out.data()),
                                 out.size() * sizeof(int16_t), FRAME_SAMPLES, bytes_written);
        check(result == micro_opus::OPUS_PACKET_DECODER_SUCCESS, "conceal_loss succeeds");
        check(bytes_written == FRAME_BYTES, "conceal_loss fills one frame");
    }

    // --- Invalid arguments ---
    {
        size_t bytes_written = 7;
        auto null_input = decoder.decode(nullptr, 10, reinterpret_cast<uint8_t*>(out.data()),
                                         out.size() * sizeof(int16_t), bytes_written);
        check(null_input == micro_opus::OPUS_PACKET_DECODER_ERROR_INPUT_INVALID,
              "null input => INPUT_INVALID");
        check(bytes_written == 0, "invalid call clears bytes_written");

        auto empty_input =
            decoder.decode(packets[0].data(), 0, reinterpret_cast<uint8_t*>(out.data()),
                           out.size() * sizeof(int16_t), bytes_written);
        check(empty_input == micro_opus::OPUS_PACKET_DECODER_ERROR_INPUT_INVALID,
              "empty input => INPUT_INVALID");
    }

    // --- reset() preserves config and keeps decoding ---
    {
        decoder.reset();
        check(decoder.get_required_output_bytes() == 0, "reset clears required_output_bytes");
        check(decoder.get_pcm_format().sample_rate() == SAMPLE_RATE, "reset preserves config");
        size_t bytes_written = 0;
        auto result = decoder.decode(packets[0].data(), packets[0].size(),
                                     reinterpret_cast<uint8_t*>(out.data()),
                                     out.size() * sizeof(int16_t), bytes_written);
        check(result == micro_opus::OPUS_PACKET_DECODER_SUCCESS, "decode works after reset");
        check(bytes_written == FRAME_BYTES, "post-reset frame size");
    }

    if (g_failures == 0) {
        std::printf("PASS: all checks passed\n");
        return 0;
    }
    std::printf("FAILED: %d check(s)\n", g_failures);
    return 1;
}
