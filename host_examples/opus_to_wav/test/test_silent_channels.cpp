/* Test for Silent Channels (value 255 in channel mapping table)
 *
 * This test verifies that the Ogg Opus decoder correctly handles
 * streams with silent channels (mapping value 255).
 */

#include "micro_opus/ogg_opus_decoder.h"

#include <cstdio>
#include <cstring>
#include <vector>

// Test stream constants
constexpr uint32_t TEST_SERIAL_NUMBER = 12345;
constexpr uint16_t TEST_PRE_SKIP = 312;       // Standard Opus pre-skip
constexpr uint32_t TEST_SAMPLE_RATE = 48000;  // Opus native sample rate
constexpr uint32_t OPUS_FRAME_SIZE = 960;     // 20ms @ 48kHz

// Ogg segment constants
constexpr size_t OGG_MAX_SEGMENT_SIZE = 255;
constexpr size_t OGG_SEGMENT_DIVISOR = 254;  // For calculating segment count

// CRC-32 lookup table (Ogg/Ethernet polynomial 0x04C11DB7)
static const uint32_t crc_lookup[256] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4};

// Helper to write little-endian integers
static void write_le32(uint8_t* buf, uint32_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
    buf[2] = (val >> 16) & 0xFF;
    buf[3] = (val >> 24) & 0xFF;
}

static void write_le16(uint8_t* buf, uint16_t val) {
    buf[0] = val & 0xFF;
    buf[1] = (val >> 8) & 0xFF;
}

// Calculate Ogg CRC32
static uint32_t calculate_crc32(const uint8_t* buffer, size_t size, uint32_t crc) {
    while (size >= 8) {
        crc ^= (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);

        crc ^= (buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7];
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);
        crc = crc_lookup[(crc >> 24) & 0xff] ^ (crc << 8);

        buffer += 8;
        size -= 8;
    }

    while (size > 0) {
        crc = (crc << 8) ^ crc_lookup[((crc >> 24) & 0xff) ^ *buffer++];
        size--;
    }

    return crc;
}

// Helper to create Ogg page header
static std::vector<uint8_t> create_ogg_page(uint8_t header_type, uint64_t granule_pos,
                                            uint32_t serial_number, uint32_t page_sequence,
                                            const std::vector<uint8_t>& packet_data,
                                            bool complete_packet = true) {
    std::vector<uint8_t> page;

    // Ogg page header
    page.push_back('O');
    page.push_back('g');
    page.push_back('g');
    page.push_back('S');
    page.push_back(0);            // Version
    page.push_back(header_type);  // Header type flags

    // Granule position (8 bytes, little-endian)
    for (int i = 0; i < 8; i++) {
        page.push_back((granule_pos >> (i * 8)) & 0xFF);
    }

    // Serial number (4 bytes)
    uint8_t serial[4];
    write_le32(serial, serial_number);
    page.insert(page.end(), serial, serial + 4);

    // Page sequence (4 bytes)
    uint8_t seq[4];
    write_le32(seq, page_sequence);
    page.insert(page.end(), seq, seq + 4);

    // Checksum position (we'll fill this in later)
    size_t checksum_pos = page.size();
    page.push_back(0);
    page.push_back(0);
    page.push_back(0);
    page.push_back(0);

    // Number of segments
    size_t num_segments = (packet_data.size() + OGG_SEGMENT_DIVISOR) / OGG_MAX_SEGMENT_SIZE;
    if (complete_packet && (packet_data.size() % OGG_MAX_SEGMENT_SIZE == 0)) {
        num_segments++;  // Need extra zero-length segment
    }
    page.push_back(num_segments);

    // Segment table
    size_t remaining = packet_data.size();
    for (size_t i = 0; i < num_segments; i++) {
        if (remaining > OGG_MAX_SEGMENT_SIZE) {
            page.push_back(OGG_MAX_SEGMENT_SIZE);
            remaining -= OGG_MAX_SEGMENT_SIZE;
        } else {
            page.push_back(remaining);
            remaining = 0;
        }
    }

    // Packet data
    page.insert(page.end(), packet_data.begin(), packet_data.end());

    // Calculate and set CRC
    uint32_t crc = calculate_crc32(page.data(), page.size(), 0);
    write_le32(&page[checksum_pos], crc);

    return page;
}

// Create OpusHead with silent channel mapping
static std::vector<uint8_t> create_opus_head_with_silent_channel() {
    std::vector<uint8_t> head;

    // Magic signature
    head.insert(head.end(), {'O', 'p', 'u', 's', 'H', 'e', 'a', 'd'});

    // Version
    head.push_back(1);

    // Channel count: 3 channels (left, right, silent center)
    head.push_back(3);

    // Pre-skip (2 bytes)
    uint8_t pre_skip[2];
    write_le16(pre_skip, TEST_PRE_SKIP);
    head.insert(head.end(), pre_skip, pre_skip + 2);

    // Input sample rate (4 bytes) - 48000 Hz
    uint8_t sample_rate[4];
    write_le32(sample_rate, TEST_SAMPLE_RATE);
    head.insert(head.end(), sample_rate, sample_rate + 4);

    // Output gain (2 bytes)
    uint8_t gain[2];
    write_le16(gain, 0);
    head.insert(head.end(), gain, gain + 2);

    // Channel mapping family: 1 (Vorbis channel order)
    head.push_back(1);

    // Stream count: 1 (only one stereo stream)
    head.push_back(1);

    // Coupled count: 1 (one coupled stereo stream)
    head.push_back(1);

    // Channel mapping table:
    // Channel 0 (left) -> stream 0, left channel (index 0)
    // Channel 1 (right) -> stream 0, right channel (index 1)
    // Channel 2 (center) -> silent (value 255)
    head.push_back(0);    // Left channel
    head.push_back(1);    // Right channel
    head.push_back(255);  // Silent channel

    return head;
}

// Create OpusTags
static std::vector<uint8_t> create_opus_tags() {
    std::vector<uint8_t> tags;

    // Magic signature
    tags.insert(tags.end(), {'O', 'p', 'u', 's', 'T', 'a', 'g', 's'});

    // Vendor string length (4 bytes)
    const char* vendor = "test";
    uint8_t vendor_len[4];
    write_le32(vendor_len, strlen(vendor));
    tags.insert(tags.end(), vendor_len, vendor_len + 4);

    // Vendor string
    tags.insert(tags.end(), vendor, vendor + strlen(vendor));

    // User comment list length (4 bytes) - 0 comments
    uint8_t comment_count[4];
    write_le32(comment_count, 0);
    tags.insert(tags.end(), comment_count, comment_count + 4);

    return tags;
}

// Create a minimal valid Opus packet (silence, ~20ms)
static std::vector<uint8_t> create_opus_packet() {
    // This is a valid Opus packet that decodes to silence
    // Byte layout: TOC, frame data...
    // 0x40: TOC byte - config 16 (SILK-only NB), stereo, 20ms, code 0
    // 0xFC, 0xFF, 0xFE: Minimal SILK frame data that decodes to silence
    // NOLINTBEGIN(readability-magic-numbers)
    return {0x40, 0xFC, 0xFF, 0xFE};
    // NOLINTEND(readability-magic-numbers)
}

int main() {
    printf("Testing Ogg Opus decoder with silent channels (value 255)...\n\n");

    // Create test stream with silent channel
    std::vector<uint8_t> stream;

    // Page 0: OpusHead (BOS)
    auto opus_head = create_opus_head_with_silent_channel();
    auto page0 = create_ogg_page(0x02, 0, TEST_SERIAL_NUMBER, 0, opus_head);
    stream.insert(stream.end(), page0.begin(), page0.end());

    // Page 1: OpusTags
    auto opus_tags = create_opus_tags();
    auto page1 = create_ogg_page(0x00, 0, TEST_SERIAL_NUMBER, 1, opus_tags);
    stream.insert(stream.end(), page1.begin(), page1.end());

    // Page 2: Audio packet
    auto opus_packet = create_opus_packet();
    auto page2 = create_ogg_page(0x00, OPUS_FRAME_SIZE, TEST_SERIAL_NUMBER, 2, opus_packet);
    stream.insert(stream.end(), page2.begin(), page2.end());

    printf("Created test stream with:\n");
    printf("  - 3 output channels (left, right, silent center)\n");
    printf("  - 1 stereo stream (left/right)\n");
    printf("  - Channel mapping: [0, 1, 255]\n");
    printf("  - Stream size: %zu bytes\n\n", stream.size());

    // Decode the stream
    micro_opus::OggOpusDecoder decoder;
    constexpr size_t NUM_CHANNELS = 3;
    int16_t pcm_buffer[OPUS_FRAME_SIZE * NUM_CHANNELS];  // 20ms @ 48kHz, 3 channels
    size_t consumed = 0;
    size_t samples_decoded = 0;
    size_t total_consumed = 0;

    printf("Decoding stream...\n");

    while (total_consumed < stream.size()) {
        micro_opus::OggOpusResult result =
            decoder.decode(stream.data() + total_consumed, stream.size() - total_consumed,
                           pcm_buffer, sizeof(pcm_buffer), consumed, samples_decoded);

        total_consumed += consumed;

        if (result != micro_opus::OGG_OPUS_OK) {
            printf("ERROR: Decode failed with code %d\n", result);
            return 1;
        }

        if (samples_decoded > 0) {
            printf("Decoded %zu samples (per channel)\n", samples_decoded);
            printf("  Sample rate: %u Hz\n", decoder.get_sample_rate());
            printf("  Channels: %u\n", decoder.get_channels());

            // Verify we got 3 channels
            if (decoder.get_channels() != 3) {
                printf("ERROR: Expected 3 channels, got %u\n", decoder.get_channels());
                return 1;
            }

            // Verify the silent channel (channel 2) is actually silent (all zeros)
            bool silent_channel_ok = true;
            for (size_t i = 0; i < samples_decoded; i++) {
                int16_t center_sample = pcm_buffer[i * 3 + 2];  // Channel 2
                if (center_sample != 0) {
                    printf("ERROR: Silent channel not silent at sample %zu: value = %d\n", i,
                           center_sample);
                    silent_channel_ok = false;
                    break;
                }
            }

            if (silent_channel_ok) {
                printf("  Silent channel verified: all samples are 0\n");
            } else {
                printf("ERROR: Silent channel contains non-zero samples!\n");
                return 1;
            }

            // Check first few samples of each channel
            printf("\nFirst 5 samples of each channel:\n");
            for (size_t i = 0; i < 5 && i < samples_decoded; i++) {
                printf("  Sample %zu: L=%d, R=%d, C=%d\n", i,
                       pcm_buffer[i * 3 + 0],   // Left
                       pcm_buffer[i * 3 + 1],   // Right
                       pcm_buffer[i * 3 + 2]);  // Center (silent)
            }
        }
    }

    printf("\nSUCCESS: All tests passed!\n");
    printf("- OpusHead with value 255 was accepted\n");
    printf("- Multistream decoder created successfully\n");
    printf("- Silent channel (255) outputs zero samples\n");
    printf("- Stream decoded without errors\n");

    return 0;
}
