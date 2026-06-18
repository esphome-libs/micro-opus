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

// Decodes an opus_demo-format ".bit" test-vector file to raw interleaved int16 PCM ("sw") using our
// OpusPacketDecoder, so the official RFC 8251 test vectors can be checked against their reference
// ".dec" decodes with opus_compare. This is the on-host conformance oracle for our patched libopus
// and for the OpusPacketDecoder wrapper itself.
//
// The .bit framing (see opus_demo.c) is a sequence of packets, each:
//   [4 bytes big-endian payload length][4 bytes big-endian encoder final range][payload bytes]
// Vectors are decoded at 48 kHz to match the reference .dec files.
//
// Usage: decode_vectors <channels:1|2> <input.bit> <output.sw>

#include "micro_opus/opus_packet_decoder.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr uint32_t kSampleRate = 48000;

uint32_t read_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) | static_cast<uint32_t>(p[3]);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::fprintf(stderr, "Usage: %s <channels:1|2> <input.bit> <output.sw>\n", argv[0]);
        return 2;
    }

    const int channels = std::atoi(argv[1]);
    if (channels != 1 && channels != 2) {
        std::fprintf(stderr, "channels must be 1 or 2\n");
        return 2;
    }

    FILE* fin = std::fopen(argv[2], "rb");
    if (fin == nullptr) {
        std::fprintf(stderr, "Cannot open input '%s'\n", argv[2]);
        return 2;
    }
    FILE* fout = std::fopen(argv[3], "wb");
    if (fout == nullptr) {
        std::fprintf(stderr, "Cannot open output '%s'\n", argv[3]);
        std::fclose(fin);
        return 2;
    }

    micro_opus::OpusPacketDecoder decoder(kSampleRate, static_cast<uint8_t>(channels));
    std::vector<uint8_t> out(decoder.get_pcm_format().max_output_bytes());
    std::vector<uint8_t> packet;

    int rc = 0;
    size_t packets = 0;
    for (;;) {
        uint8_t header[8];
        const size_t got = std::fread(header, 1, sizeof(header), fin);
        if (got == 0) {
            break;  // Clean end of file
        }
        if (got != sizeof(header)) {
            std::fprintf(stderr, "Truncated packet header (%zu bytes)\n", got);
            rc = 1;
            break;
        }

        const uint32_t len = read_be32(header);  // header[0..3] = length; [4..7] = range (unused)
        // A single Opus packet is at most ~61 KB (code 3, 48 frames of 1275 bytes). A length
        // outside that range means a corrupt or unsupported bitstream. The RFC vectors contain
        // neither a zero-length (DTX/PLC) packet nor an oversized one, so treat both as hard errors
        // rather than concealing or allocating a huge buffer.
        constexpr uint32_t kMaxPacketBytes = 61440;
        if (len == 0) {
            std::fprintf(stderr, "Zero-length (DTX) packets are not supported by this harness\n");
            rc = 1;
            break;
        }
        if (len > kMaxPacketBytes) {
            std::fprintf(stderr, "Implausible payload length %u (> %u)\n", len, kMaxPacketBytes);
            rc = 1;
            break;
        }

        packet.resize(len);
        if (std::fread(packet.data(), 1, len, fin) != len) {
            std::fprintf(stderr, "Ran out of input reading %u-byte payload\n", len);
            rc = 1;
            break;
        }

        size_t bytes_written = 0;
        const auto result =
            decoder.decode(packet.data(), packet.size(), out.data(), out.size(), bytes_written);
        if (result != micro_opus::OPUS_PACKET_DECODER_SUCCESS) {
            std::fprintf(stderr, "Decode failed on packet %zu: %d\n", packets, result);
            rc = 1;
            break;
        }

        if (bytes_written > 0 && std::fwrite(out.data(), 1, bytes_written, fout) != bytes_written) {
            std::fprintf(stderr, "Write error\n");
            rc = 1;
            break;
        }
        ++packets;
    }

    std::fclose(fin);
    std::fclose(fout);
    if (rc == 0) {
        std::fprintf(stderr, "decoded %zu packets\n", packets);
    }
    return rc;
}
