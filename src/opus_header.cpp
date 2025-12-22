// Copyright 2025 Kevin Ahrendt
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

/* Opus Header Parsing for Ogg Opus Streams
 * Implements RFC 7845 OpusHead and OpusTags parsing
 */

#include "opus_header.h"

#include <cstring>

namespace micro_opus {

namespace {
// Little-endian helpers for Opus header parsing
inline uint16_t read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

inline uint32_t read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

// RFC 7845: Opus magic signature lengths
// Section 5.1: OpusHead begins with "OpusHead" (8 bytes)
// Section 5.2: OpusTags begins with "OpusTags" (8 bytes)
const size_t OPUS_MAGIC_SIGNATURE_SIZE = 8;

// RFC 7845 Section 5.1: Minimum OpusHead size
// Must contain: magic(8) + version(1) + channel_count(1) + pre_skip(2) +
//               input_sample_rate(4) + output_gain(2) + channel_mapping(1) = 19 bytes
const size_t MIN_OPUS_HEAD_SIZE = 19;

// RFC 7845 Section 5.1.1: Minimum OpusHead size with channel mapping
// When channel_mapping != 0, need additional: stream_count(1) + coupled_count(1) = 21 bytes
const size_t MIN_OPUS_HEAD_SIZE_WITH_MAPPING = 21;

// OpusHead field offsets (relative to end of magic signature)
// RFC 7845 Section 5.1 defines the header layout
constexpr size_t OPUS_HEAD_CHANNEL_MAPPING_OFFSET = 10;  // channel_mapping field
constexpr size_t OPUS_HEAD_STREAM_COUNT_OFFSET = 11;     // stream_count field (if mapping != 0)
constexpr size_t OPUS_HEAD_COUPLED_COUNT_OFFSET = 12;    // coupled_count field (if mapping != 0)
}  // namespace

bool is_opus_head(const uint8_t* packet, size_t packet_len) {
    if (packet_len < OPUS_MAGIC_SIGNATURE_SIZE) {
        return false;
    }
    return memcmp(packet, "OpusHead", OPUS_MAGIC_SIGNATURE_SIZE) == 0;
}

bool is_opus_tags(const uint8_t* packet, size_t packet_len) {
    if (packet_len < OPUS_MAGIC_SIGNATURE_SIZE) {
        return false;
    }
    return memcmp(packet, "OpusTags", OPUS_MAGIC_SIGNATURE_SIZE) == 0;
}

OpusHeaderResult parse_opus_head(const uint8_t* packet, size_t packet_len, OpusHead& head) {
    // Check magic signature
    if (!is_opus_head(packet, packet_len)) {
        return OPUS_HEADER_INVALID_MAGIC;
    }

    // Minimum header size for channel mapping 0
    if (packet_len < MIN_OPUS_HEAD_SIZE) {
        return OPUS_HEADER_TOO_SHORT;
    }

    // Parse header fields (offsets relative to end of magic signature)
    head.version = packet[OPUS_MAGIC_SIGNATURE_SIZE + 0];
    head.channel_count = packet[OPUS_MAGIC_SIGNATURE_SIZE + 1];
    head.pre_skip = read_le16(packet + OPUS_MAGIC_SIGNATURE_SIZE + 2);
    head.input_sample_rate = read_le32(packet + OPUS_MAGIC_SIGNATURE_SIZE + 4);
    head.output_gain = (int16_t)read_le16(packet + OPUS_MAGIC_SIGNATURE_SIZE + 8);
    head.channel_mapping = packet[OPUS_MAGIC_SIGNATURE_SIZE + OPUS_HEAD_CHANNEL_MAPPING_OFFSET];

    // Validate version
    if (head.version != 1) {
        return OPUS_HEADER_INVALID_VERSION;
    }

    // Validate channel count
    if (head.channel_count == 0) {
        return OPUS_HEADER_INVALID_CHANNELS;
    }

    // RFC 7845 Section 5.1: input_sample_rate is "informational only"
    // Per RFC: "Decoders MAY use this value to select an initial output sample rate,
    // but SHOULD NOT reject the stream if it has an unexpected value."
    // Therefore, we parse it but do not validate it.

    // Parse channel mapping table if needed
    if (head.channel_mapping != 0) {
        // Need additional bytes for channel mapping
        if (packet_len < MIN_OPUS_HEAD_SIZE_WITH_MAPPING + head.channel_count) {
            return OPUS_HEADER_TOO_SHORT;
        }

        head.stream_count = packet[OPUS_MAGIC_SIGNATURE_SIZE + OPUS_HEAD_STREAM_COUNT_OFFSET];
        head.coupled_count = packet[OPUS_MAGIC_SIGNATURE_SIZE + OPUS_HEAD_COUPLED_COUNT_OFFSET];

        // Copy channel mapping table
        memcpy(head.channel_mapping_table, packet + MIN_OPUS_HEAD_SIZE_WITH_MAPPING,
               head.channel_count);

        // RFC 7845 Section 5.1.1: Validate channel mapping table indices
        // Each mapping value must be a valid stream index (< stream_count + coupled_count)
        // OR 255 which indicates a silent channel
        // Use uint16_t to prevent integer overflow (two uint8_t values can sum to 510)
        uint16_t total_streams = (uint16_t)head.stream_count + (uint16_t)head.coupled_count;
        for (uint8_t i = 0; i < head.channel_count; i++) {
            // Accept values < total_streams OR 255 (silent channel)
            if (head.channel_mapping_table[i] >= total_streams &&
                head.channel_mapping_table[i] != 255) {
                return OPUS_HEADER_INVALID_MAPPING;
            }
        }
    } else {
        // Channel mapping family 0
        head.stream_count = 1;
        head.coupled_count = (head.channel_count == 2) ? 1 : 0;
        memset(head.channel_mapping_table, 0, sizeof(head.channel_mapping_table));
    }

    // Validate channel mapping family constraints (RFC 7845 Section 5.1.1)
    if (head.channel_mapping == 0) {
        // Family 0 (RTP/Stereo): Only 1-2 channels allowed
        if (head.channel_count > 2) {
            return OPUS_HEADER_INVALID_CHANNELS;
        }
        // stream_count and coupled_count already set correctly above

    } else if (head.channel_mapping == 1) {
        // Family 1 (Vorbis/Surround): 1-8 channels for surround sound
        if (head.channel_count > 8) {
            return OPUS_HEADER_INVALID_CHANNELS;
        }

        // Validate stream and coupled counts (RFC 7845 Section 5.1.1)
        if (head.stream_count == 0) {
            return OPUS_HEADER_INVALID_MAPPING;
        }
        if (head.coupled_count > head.stream_count) {
            return OPUS_HEADER_INVALID_MAPPING;
        }

    } else {
        // Families 2-254 are reserved, family 255 is undefined/experimental
        // RFC 7845 Section 5.1.1.4: "A demuxer implementation encountering a reserved
        // 'channel mapping family' value SHOULD act as though the value is 255."
        // We accept these families but validate basic constraints
        if (head.stream_count == 0) {
            return OPUS_HEADER_INVALID_MAPPING;
        }
        if (head.coupled_count > head.stream_count) {
            return OPUS_HEADER_INVALID_MAPPING;
        }
    }

    return OPUS_HEADER_OK;
}

}  // namespace micro_opus
