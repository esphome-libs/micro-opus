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

// Verifies OggOpusDecoder handles channel mapping family 1 with a silent channel (mapping value
// 255): the stream declares 3 output channels (L, R, silent center) backed by one coupled stereo
// stream, and the center channel must decode to all zeros. Synthesizes the Ogg stream in memory via
// the shared ogg_mux helpers.

#include "micro_opus/ogg_opus_decoder.h"
#include "ogg_mux.h"
#include "opus.h"

#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

constexpr uint32_t SERIAL_NUMBER = 12345;
constexpr uint32_t FRAME_SIZE = 960;  // 20 ms @ 48 kHz

using micro_opus_test::make_ogg_page;
using micro_opus_test::make_opus_head_family1;
using micro_opus_test::make_opus_tags;
using micro_opus_test::OGG_FLAG_BOS;

// Encode one 20 ms stereo frame of silence with libopus. The OpusHead below declares a single
// coupled (stereo) stream, so its packets must be well-formed stereo Opus packets. Returns empty on
// failure.
std::vector<uint8_t> make_silent_stereo_packet() {
    int err = 0;
    OpusEncoder* enc = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &err);
    if (enc == nullptr || err != OPUS_OK) {
        return {};
    }
    const std::vector<int16_t> silence(static_cast<size_t>(FRAME_SIZE) * 2, 0);
    std::vector<uint8_t> packet(4000);
    const int bytes = opus_encode(enc, silence.data(), static_cast<int>(FRAME_SIZE), packet.data(),
                                  static_cast<opus_int32>(packet.size()));
    opus_encoder_destroy(enc);
    if (bytes < 0) {
        return {};
    }
    packet.resize(static_cast<size_t>(bytes));
    return packet;
}

}  // namespace

int main() {
    std::printf("Testing Ogg Opus decoder with silent channels (value 255)...\n\n");

    // Channel mapping table: [0, 1, 255] -> L, R, silent center.
    const std::vector<uint8_t> mapping = {0, 1, 255};
    const auto opus_head = make_opus_head_family1(/*channels=*/3, /*stream_count=*/1,
                                                  /*coupled_count=*/1, mapping);

    const std::vector<uint8_t> audio_packet = make_silent_stereo_packet();
    if (audio_packet.empty()) {
        std::printf("ERROR: failed to encode silent stereo packet\n");
        return 1;
    }

    std::vector<uint8_t> stream;
    auto append = [&stream](const std::vector<uint8_t>& page) {
        stream.insert(stream.end(), page.begin(), page.end());
    };
    append(make_ogg_page(OGG_FLAG_BOS, 0, SERIAL_NUMBER, 0, opus_head));
    append(make_ogg_page(0x00, 0, SERIAL_NUMBER, 1, make_opus_tags()));
    append(make_ogg_page(0x00, FRAME_SIZE, SERIAL_NUMBER, 2, audio_packet));

    std::printf("Created test stream: 3 channels [0,1,255], 1 coupled stereo stream, %zu bytes\n\n",
                stream.size());

    micro_opus::OggOpusDecoder decoder;
    constexpr size_t NUM_CHANNELS = 3;
    int16_t pcm_buffer[FRAME_SIZE * NUM_CHANNELS];
    size_t consumed = 0;
    size_t samples_decoded = 0;
    size_t total_consumed = 0;
    size_t total_decoded = 0;

    while (total_consumed < stream.size()) {
        micro_opus::OggOpusResult result = decoder.decode(
            stream.data() + total_consumed, stream.size() - total_consumed,
            reinterpret_cast<uint8_t*>(pcm_buffer), sizeof(pcm_buffer), consumed, samples_decoded);
        total_consumed += consumed;

        if (result != micro_opus::OGG_OPUS_OK) {
            std::printf("ERROR: Decode failed with code %d\n", result);
            return 1;
        }

        if (consumed == 0 && samples_decoded == 0) {
            std::printf("ERROR: decoder made no progress (consumed 0 bytes, produced 0 samples)\n");
            return 1;
        }

        if (samples_decoded > 0) {
            std::printf("Decoded %zu samples/channel, %u channels @ %u Hz\n", samples_decoded,
                        decoder.get_channels(), decoder.get_sample_rate());

            if (decoder.get_channels() != NUM_CHANNELS) {
                std::printf("ERROR: Expected 3 channels, got %u\n", decoder.get_channels());
                return 1;
            }

            for (size_t i = 0; i < samples_decoded; ++i) {
                const int16_t center = pcm_buffer[i * NUM_CHANNELS + 2];
                if (center != 0) {
                    std::printf("ERROR: Silent channel not silent at sample %zu: %d\n", i, center);
                    return 1;
                }
            }
            std::printf("  Silent channel verified: all samples are 0\n");
            total_decoded += samples_decoded;
        }
    }

    if (total_decoded == 0) {
        std::printf("ERROR: no audio samples decoded; the silent-channel check never ran\n");
        return 1;
    }

    std::printf("\nPASS: silent channel (255) decoded as zeros across a 3-channel stream\n");
    return 0;
}
