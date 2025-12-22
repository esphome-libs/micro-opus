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

#ifndef OPUS_HEADER_H
#define OPUS_HEADER_H

#include <stddef.h>
#include <stdint.h>

namespace micro_opus {

/**
 * @brief OpusHead header structure (per RFC 7845 Section 5.1)
 */
struct OpusHead {
    uint8_t version;             // Version (must be 1)
    uint8_t channel_count;       // Number of channels (1-255)
    uint16_t pre_skip;           // Pre-skip samples at 48kHz
    uint32_t input_sample_rate;  // Original sample rate (informational)
    int16_t output_gain;         // Output gain in Q7.8 dB
    uint8_t channel_mapping;     // Channel mapping family (0 or 1)

    // Channel mapping table (only if channel_mapping != 0)
    uint8_t stream_count;                // Number of encoded streams
    uint8_t coupled_count;               // Number of coupled streams
    uint8_t channel_mapping_table[255];  // Channel mapping
};

/**
 * @brief Result codes for header parsing
 */
enum OpusHeaderResult {
    OPUS_HEADER_OK = 0,
    OPUS_HEADER_INVALID_MAGIC = -1,
    OPUS_HEADER_INVALID_VERSION = -2,
    OPUS_HEADER_TOO_SHORT = -3,
    OPUS_HEADER_INVALID_CHANNELS = -4,
    OPUS_HEADER_INVALID_MAPPING = -5  // For invalid stream/coupled counts
};

/**
 * @brief Parse OpusHead header packet
 *
 * @param packet Packet data (must start with "OpusHead")
 * @param packet_len Packet length in bytes
 * @param head Output OpusHead structure
 * @return OpusHeaderResult result code
 */
OpusHeaderResult parse_opus_head(const uint8_t* packet, size_t packet_len, OpusHead& head);

/**
 * @brief Check if packet is OpusHead
 *
 * @param packet Packet data
 * @param packet_len Packet length
 * @return true if packet starts with "OpusHead"
 */
bool is_opus_head(const uint8_t* packet, size_t packet_len);

/**
 * @brief Check if packet is OpusTags
 *
 * @param packet Packet data
 * @param packet_len Packet length
 * @return true if packet starts with "OpusTags"
 */
bool is_opus_tags(const uint8_t* packet, size_t packet_len);

}  // namespace micro_opus

#endif  // OPUS_HEADER_H
