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

/* Ogg Opus Streaming Decoder Wrapper for ESP-IDF
 * Implementation of OggOpusDecoder class
 */

#include "micro_opus/ogg_opus_decoder.h"

#include "opus.h"
#include "opus_header.h"
#include "opus_multistream.h"
#include <micro_ogg/ogg_demuxer.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#else
#include <cstdio>
#include <cstdlib>
// Host compatibility: map heap_caps functions to standard malloc/free
#define HEAP_CAPS_MALLOC(size, caps) malloc(size)
#define HEAP_CAPS_FREE(ptr) free(ptr)
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_INTERNAL 0
#endif

#include <algorithm>
#include <climits>
#include <cstring>

namespace micro_opus {

namespace {
// RFC 3533: Invalid/unknown granule position (-1 in two's complement)
constexpr uint64_t INVALID_GRANULE_POSITION = 0xFFFFFFFFFFFFFFFFULL;

// RFC 6716 Section 2: Valid Opus sample rates
// Opus supports decoding at these rates: 8000, 12000, 16000, 24000, 48000 Hz
constexpr uint32_t OPUS_SAMPLE_RATE_8K = 8000;
constexpr uint32_t OPUS_SAMPLE_RATE_12K = 12000;
constexpr uint32_t OPUS_SAMPLE_RATE_16K = 16000;
constexpr uint32_t OPUS_SAMPLE_RATE_24K = 24000;
constexpr uint32_t OPUS_SAMPLE_RATE_48K = 48000;  // Native Opus rate

// RFC 7845 Section 3: Opus packet sizes
// "Demuxers SHOULD treat audio packets > 61,440 octets as invalid"
// Typical packets at 128kbps/20ms are ~320 bytes, so start small and grow as needed
const size_t min_opus_packet_size = 1024;   // Initial buffer allocation
const size_t max_opus_packet_size = 61440;  // Maximum per RFC 7845

// RFC 7845 Section 5.2: Maximum OpusTags size
// "OpusTags MUST NOT exceed 125,829,120 octets (120 MB)"
// This prevents memory exhaustion attacks
const size_t max_opus_tags_size = 125829120;

// RFC 7845 Section 5.2: Minimum OpusTags size
// Must contain: magic(8) + vendor_length(4) + user_comment_count(4) = 16 bytes
const size_t min_opus_tags_size = 16;
}  // namespace

OggOpusResult OggOpusDecoder::process_packet(const micro_ogg::OggPacket& packet, int16_t* output,
                                             size_t output_size, size_t& samples_decoded) {
    // Extract packet data
    const uint8_t* packet_data = packet.data;
    size_t packet_len = packet.length;
    int64_t granule_pos = packet.granule_position;
    bool is_bos = packet.is_bos;
    bool is_eos = packet.is_eos;
    bool is_last_on_page = packet.is_last_on_page;

    // RFC 7845 Section 4.1: Validate continued packet flag consistency
    // Only check on first packet of a new page
    if (packets_on_current_page_ == 0) {
        bool page_has_continued_flag = ogg_demuxer_->currentPageHasContinuedFlag();
        if (page_has_continued_flag != expect_continued_packet_) {
            return OGG_OPUS_INPUT_INVALID;
        }
    }

    // Dispatch to state handler
    switch (state_) {
        case STATE_EXPECT_OPUS_HEAD:
            samples_decoded = 0;
            return handle_opus_head_packet(packet_data, packet_len, granule_pos, is_bos,
                                           is_last_on_page);

        case STATE_EXPECT_OPUS_TAGS:
            samples_decoded = 0;
            return handle_opus_tags_packet(packet_data, packet_len, granule_pos, is_last_on_page);

        case STATE_DECODING:
            return handle_audio_packet(packet_data, packet_len, granule_pos, is_eos,
                                       is_last_on_page, output, output_size, samples_decoded);
    }

    // Unreachable with valid enum, but satisfy compiler
    return OGG_OPUS_INPUT_INVALID;
}

void OggOpusDecoder::update_page_tracking(bool is_last_on_page) {
    packets_on_current_page_++;
    if (is_last_on_page) {
        packets_on_current_page_ = 0;
        expect_continued_packet_ = ogg_demuxer_->previousPageEndedWithContinuedPacket();
        previous_packet_was_last_on_page_ = true;
    } else {
        previous_packet_was_last_on_page_ = false;
    }
}

OggOpusResult OggOpusDecoder::validate_granule_position(int64_t granule_pos, size_t decoded_samples,
                                                        bool is_eos, bool is_last_on_page) {
    if (granule_pos > 0 && (uint64_t)granule_pos != INVALID_GRANULE_POSITION) {
        // RFC 7845 Section 4: First audio data page granule position validation
        if (first_audio_page_samples_ == -1 && last_granule_position_ == 0) {
            first_audio_page_samples_ = 0;
        }

        if (first_audio_page_samples_ >= 0) {
            first_audio_page_samples_ += decoded_samples;

            if (is_last_on_page) {
                if (!is_eos && granule_pos < first_audio_page_samples_) {
                    return OGG_OPUS_INPUT_INVALID;
                }
                first_audio_page_samples_ = -1;
            }
        }

        // Validate monotonically increasing granule position
        if (last_granule_position_ > 0) {
            if (granule_pos < last_granule_position_) {
                return OGG_OPUS_INPUT_INVALID;
            }
        }
        last_granule_position_ = granule_pos;
    }
    return OGG_OPUS_OK;
}

OggOpusResult OggOpusDecoder::create_opus_decoder(uint8_t output_channels) {
    int error = 0;
    if (opus_head_->channel_mapping == 0) {
        opus_decoder_ = opus_decoder_create(sample_rate_, output_channels, &error);

        if (error != OPUS_OK || !opus_decoder_) {
            return OGG_OPUS_ALLOCATION_FAILED;
        }

        if (opus_head_->output_gain != 0) {
            opus_decoder_ctl(opus_decoder_, OPUS_SET_GAIN((opus_int32)opus_head_->output_gain));
        }
    } else {
        opus_ms_decoder_ = opus_multistream_decoder_create(
            sample_rate_, output_channels, opus_head_->stream_count, opus_head_->coupled_count,
            opus_head_->channel_mapping_table, &error);

        if (error != OPUS_OK || !opus_ms_decoder_) {
            return OGG_OPUS_ALLOCATION_FAILED;
        }

        if (opus_head_->output_gain != 0) {
            opus_multistream_decoder_ctl(opus_ms_decoder_,
                                         OPUS_SET_GAIN((opus_int32)opus_head_->output_gain));
        }
    }
    return OGG_OPUS_OK;
}

OggOpusResult OggOpusDecoder::handle_opus_head_packet(const uint8_t* packet_data, size_t packet_len,
                                                      int64_t granule_pos, bool is_bos,
                                                      bool is_last_on_page) {
    // First packet should be OpusHead and must have BOS flag
    if (!is_bos || !is_opus_head(packet_data, packet_len)) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // RFC 7845 Section 4: First Ogg page MUST contain only the OpusHead packet
    if (has_seen_opus_head_) {
        return OGG_OPUS_INPUT_INVALID;
    }
    has_seen_opus_head_ = true;

    // RFC 7845 Section 4: OpusHead must be alone on page 0
    if (is_last_on_page && packets_on_current_page_ != 0) {
        return OGG_OPUS_INPUT_INVALID;
    }
    update_page_tracking(is_last_on_page);

    // Lazy allocation: allocate OpusHead structure when needed
    if (!opus_head_) {
        opus_head_ = std::make_unique<OpusHead>();
    }

    OpusHeaderResult header_result = parse_opus_head(packet_data, packet_len, *opus_head_);

    if (header_result != OPUS_HEADER_OK) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // Validate granule position for OpusHead page
    if (granule_pos != 0) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // Determine output channel count: use configured value or file's channel count
    output_channels_ = (channels_ != 0) ? channels_ : opus_head_->channel_count;

    // Create Opus decoder
    OggOpusResult decoder_result = create_opus_decoder(output_channels_);
    if (decoder_result != OGG_OPUS_OK) {
        return decoder_result;
    }

    state_ = STATE_EXPECT_OPUS_TAGS;
    return OGG_OPUS_OK;
}

OggOpusResult OggOpusDecoder::handle_opus_tags_packet(const uint8_t* packet_data, size_t packet_len,
                                                      int64_t granule_pos, bool is_last_on_page) {
    // Second packet should be OpusTags
    if (!is_opus_tags(packet_data, packet_len)) {
        return OGG_OPUS_INPUT_INVALID;
    }

    if (has_seen_opus_tags_) {
        return OGG_OPUS_INPUT_INVALID;
    }
    has_seen_opus_tags_ = true;

    // RFC 7845 Section 5.2: OpusTags MUST NOT exceed 125,829,120 octets
    opus_tags_accumulated_size_ += packet_len;
    if (opus_tags_accumulated_size_ > max_opus_tags_size) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // RFC 7845 Section 5.2: Minimum OpusTags size validation
    if (packet_len < min_opus_tags_size) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // RFC 7845 Section 4: OpusTags must be alone on the page where it completes
    if (is_last_on_page) {
        if (packets_on_current_page_ != 0) {
            return OGG_OPUS_INPUT_INVALID;
        }
        // Validate granule position for OpusTags page
        if (granule_pos != 0) {
            return OGG_OPUS_INPUT_INVALID;
        }
    }
    update_page_tracking(is_last_on_page);

    state_ = STATE_DECODING;
    return OGG_OPUS_OK;
}

OggOpusResult OggOpusDecoder::handle_audio_packet(const uint8_t* packet_data, size_t packet_len,
                                                  int64_t granule_pos, bool is_eos,
                                                  bool is_last_on_page, int16_t* output,
                                                  size_t output_size, size_t& samples_decoded) {
    // RFC 7845 Section 3: Mark EOS seen
    if (is_eos) {
        eos_seen_ = true;
    }

    // RFC 7845 Section 4.1: MUST treat zero-octet audio data packet as malformed
    if (packet_len == 0) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // RFC 7845 Section 3: Audio data packets SHOULD NOT exceed 61,440 octets
    if (packet_len > max_opus_packet_size) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // Calculate required buffer size for this packet
    int nb_samples =
        opus_packet_get_nb_samples(packet_data, (opus_int32)packet_len, (opus_int32)sample_rate_);

    if (nb_samples > 0) {
        size_t required_samples = static_cast<size_t>(nb_samples);
        last_required_buffer_bytes_ = required_samples * output_channels_ * sizeof(int16_t);

        // Check if output buffer is large enough
        if (output_size < last_required_buffer_bytes_) {
            return OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL;
        }
    }

    size_t max_samples = output_size / (output_channels_ * sizeof(int16_t));
    int max_frame_size = (int)std::min(max_samples, (size_t)INT_MAX);

    // Decode Opus packet
    int decoded_samples_int = 0;
    if (opus_decoder_) {
        decoded_samples_int =
            opus_decode(opus_decoder_, packet_data, (opus_int32)packet_len, output, max_frame_size,
                        0  // No FEC
            );
    } else if (opus_ms_decoder_) {
        decoded_samples_int = opus_multistream_decode(
            opus_ms_decoder_, packet_data, (opus_int32)packet_len, output, max_frame_size,
            0  // No FEC
        );
    } else {
        return OGG_OPUS_NOT_INITIALIZED;
    }

    if (decoded_samples_int < 0) {
        return OGG_OPUS_DECODE_ERROR;
    }
    size_t decoded_samples_size = (size_t)decoded_samples_int;

    update_page_tracking(is_last_on_page);

    OggOpusResult granule_result =
        validate_granule_position(granule_pos, decoded_samples_size, is_eos, is_last_on_page);
    if (granule_result != OGG_OPUS_OK) {
        return granule_result;
    }

    return apply_pre_skip(output, decoded_samples_size, output_channels_, samples_decoded);
}

OggOpusResult OggOpusDecoder::apply_pre_skip(int16_t* output, size_t decoded_samples,
                                             uint8_t output_channels, size_t& samples_decoded) {
    if (!pre_skip_applied_ && opus_head_->pre_skip > 0) {
        // Validate sample_rate_ is one of the allowed Opus sample rates
        if (sample_rate_ != OPUS_SAMPLE_RATE_8K && sample_rate_ != OPUS_SAMPLE_RATE_12K &&
            sample_rate_ != OPUS_SAMPLE_RATE_16K && sample_rate_ != OPUS_SAMPLE_RATE_24K &&
            sample_rate_ != OPUS_SAMPLE_RATE_48K) {
            return OGG_OPUS_INPUT_INVALID;
        }

        // Convert pre-skip from 48kHz units to current sample rate
        uint64_t pre_skip_at_sample_rate =
            ((uint64_t)opus_head_->pre_skip * (uint64_t)sample_rate_) / OPUS_SAMPLE_RATE_48K;

        if (samples_decoded_total_ + decoded_samples <= pre_skip_at_sample_rate) {
            // Entire frame is within pre-skip range
            samples_decoded_total_ += decoded_samples;
            samples_decoded = 0;
            return OGG_OPUS_OK;
        }
        if (samples_decoded_total_ < pre_skip_at_sample_rate) {
            // Partial frame needs to be skipped
            size_t skip_count = pre_skip_at_sample_rate - samples_decoded_total_;

            if (skip_count > decoded_samples) {
                return OGG_OPUS_INPUT_INVALID;
            }

            size_t keep_count = decoded_samples - skip_count;

            // Shift samples to remove skipped portion
            memmove(output, output + (skip_count * output_channels),
                    keep_count * output_channels * sizeof(int16_t));

            samples_decoded_total_ += decoded_samples;
            samples_decoded = keep_count;
            pre_skip_applied_ = true;
            return OGG_OPUS_OK;
        }

        pre_skip_applied_ = true;
    }

    samples_decoded_total_ += decoded_samples;
    samples_decoded = decoded_samples;
    return OGG_OPUS_OK;
}

OggOpusDecoder::OggOpusDecoder(bool enable_crc, uint32_t sample_rate, uint8_t channels)
    : enable_crc_(enable_crc),
      sample_rate_(sample_rate),
      channels_(channels),
      ogg_demuxer_(nullptr),
      opus_head_(nullptr) {
    // Lazy allocation: all resources allocated on first decode() call
    // Constructor guaranteed to succeed
    // Note: sample_rate validation happens at decoder creation time in processPacket()
}

OggOpusDecoder::~OggOpusDecoder() {
    if (opus_decoder_) {
        opus_decoder_destroy(opus_decoder_);
        opus_decoder_ = nullptr;
    }

    if (opus_ms_decoder_) {
        opus_multistream_decoder_destroy(opus_ms_decoder_);
        opus_ms_decoder_ = nullptr;
    }

    // ogg_demuxer_ and opus_head_ are automatically cleaned up by unique_ptr
}

void OggOpusDecoder::reset() {
    if (opus_decoder_) {
        opus_decoder_destroy(opus_decoder_);
        opus_decoder_ = nullptr;
    }

    if (opus_ms_decoder_) {
        opus_multistream_decoder_destroy(opus_ms_decoder_);
        opus_ms_decoder_ = nullptr;
    }

    if (ogg_demuxer_) {
        ogg_demuxer_->reset();
    }

    // Reset opus_head_ smart pointer
    opus_head_.reset();

    state_ = STATE_EXPECT_OPUS_HEAD;
    // Note: sample_rate_ and channels_ are NOT reset - they are configuration values
    output_channels_ = 0;  // Will be set after next OpusHead parsing
    samples_decoded_total_ = 0;
    pre_skip_applied_ = false;
    last_granule_position_ = 0;
    last_required_buffer_bytes_ = 0;
    has_seen_opus_head_ = false;
    has_seen_opus_tags_ = false;
    opus_tags_accumulated_size_ = 0;
    packets_on_current_page_ = 0;
    first_audio_page_samples_ = -1;  // -1 = not yet on first audio page
    eos_seen_ = false;
    expect_continued_packet_ = false;
    previous_packet_was_last_on_page_ = true;  // First packet after reset is on a new page
}

uint32_t OggOpusDecoder::get_sample_rate() const {
    return (state_ == STATE_DECODING) ? sample_rate_ : 0;
}

uint8_t OggOpusDecoder::get_channels() const {
    return output_channels_;
}

uint8_t OggOpusDecoder::get_bit_depth() const {
    return 16;  // Opus decoder always outputs 16-bit samples
}

uint8_t OggOpusDecoder::get_bytes_per_sample() const {
    return 2;  // sizeof(int16_t)
}

uint16_t OggOpusDecoder::get_pre_skip() const {
    // Only return valid pre-skip after OpusHead has been parsed
    return (state_ == STATE_DECODING && opus_head_) ? opus_head_->pre_skip : 0;
}

int16_t OggOpusDecoder::get_output_gain() const {
    // Only return valid output gain after OpusHead has been parsed
    return (state_ == STATE_DECODING && opus_head_) ? opus_head_->output_gain : 0;
}

size_t OggOpusDecoder::get_required_output_buffer_size() const {
    return last_required_buffer_bytes_;
}

#ifdef MICRO_OGG_DEMUXER_DEBUG
void OggOpusDecoder::get_demuxer_debug_state(int& state, bool& assembling, bool& skipping,
                                             size_t& packet_size, size_t& body_consumed,
                                             uint8_t& seg_index, uint8_t& seg_count) const {
    if (ogg_demuxer_) {
        ogg_demuxer_->getDebugState(state, assembling, skipping, packet_size, body_consumed,
                                    seg_index, seg_count);
    } else {
        state = -1;
        assembling = false;
        skipping = false;
        packet_size = 0;
        body_consumed = 0;
        seg_index = 0;
        seg_count = 0;
    }
}
#endif  // MICRO_OGG_DEMUXER_DEBUG

bool OggOpusDecoder::is_initialized() const {
    return state_ == STATE_DECODING;
}

OggOpusResult OggOpusDecoder::decode(const uint8_t* input, size_t input_len, int16_t* output,
                                     size_t output_size, size_t& bytes_consumed,
                                     size_t& samples_decoded) {
    // Validate input pointer
    if (!input) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // Validate output buffer only when decoding audio (not during header parsing)
    if (state_ == STATE_DECODING) {
        if (!output) {
            return OGG_OPUS_INPUT_INVALID;
        }
        if (output_size == 0) {
            return OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL;
        }
    }

    // RFC 7845 Section 3: Enforce end of stream validation
    // "There MUST NOT be any more pages in an Opus logical bitstream after a page marked 'end of
    // stream'." This is a codec-specific requirement (Opus), not a container requirement (Ogg
    // general).
    if (eos_seen_) {
        return OGG_OPUS_INPUT_INVALID;
    }

    // Lazy allocation: create demuxer on first use
    if (!ogg_demuxer_) {
        // RFC 7845 Section 3: Typical Opus packets are ~320 bytes
        // Maximum packet size is 61,440 octets per RFC 7845
        micro_ogg::OggDemuxerConfig ogg_config;
        ogg_config.min_buffer_size = min_opus_packet_size;
        ogg_config.max_buffer_size = max_opus_packet_size;
        ogg_config.enable_crc = enable_crc_;

#ifdef ESP_PLATFORM
        // Use preference-aware allocators on ESP32 (configurable via Kconfig)
        struct AllocFns {
            static void* alloc_fn(size_t size) {
#if defined(CONFIG_OPUS_OGG_DECODER_PREFER_PSRAM)
                return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_OGG_DECODER_PREFER_INTERNAL)
                return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_OGG_DECODER_PSRAM_ONLY)
                return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_OGG_DECODER_INTERNAL_ONLY)
                return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
                // Default: prefer PSRAM with fallback to internal RAM
                return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                               MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
            }
            static void* realloc_fn(void* ptr, size_t size) {
#if defined(CONFIG_OPUS_OGG_DECODER_PREFER_PSRAM)
                return heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_OGG_DECODER_PREFER_INTERNAL)
                return heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_OGG_DECODER_PSRAM_ONLY)
                return heap_caps_realloc(ptr, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_OGG_DECODER_INTERNAL_ONLY)
                return heap_caps_realloc(ptr, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
                // Default: prefer PSRAM with fallback to internal RAM
                return heap_caps_realloc_prefer(ptr, size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
            }
            static void free_fn(void* ptr) {
                heap_caps_free(ptr);
            }
        };
        ogg_config.alloc = AllocFns::alloc_fn;
        ogg_config.realloc = AllocFns::realloc_fn;
        ogg_config.free = AllocFns::free_fn;
#endif

        ogg_demuxer_ = std::make_unique<micro_ogg::OggDemuxer>(ogg_config);
    }

    bytes_consumed = 0;
    samples_decoded = 0;

    // Get next packet from demuxer
    micro_ogg::OggDemuxState parse_state = ogg_demuxer_->getNextPacket(input, input_len);
    bytes_consumed = parse_state.bytes_consumed;

    // Handle demuxer results
    if (parse_state.result == micro_ogg::OGG_NEED_MORE_DATA) {
        return OGG_OPUS_OK;
    }

    if (parse_state.result == micro_ogg::OGG_PACKET_SKIPPED) {
        // Packet was skipped (too large to buffer)
        // This is acceptable for OpusTags packets - transition to decoding state
        if (state_ == STATE_EXPECT_OPUS_TAGS) {
            // OpusTags was skipped due to size (e.g., large album art)
            // Mark it as seen and transition to decoding state
            has_seen_opus_tags_ = true;
            state_ = STATE_DECODING;
        }
        // For audio packets, skipping is an error (they shouldn't exceed max_buffer_size)
        // But the demuxer already validated this, so just return OK
        return OGG_OPUS_OK;
    }

    if (parse_state.result == micro_ogg::OGG_OK) {
        // We have a complete packet - process it
        OggOpusResult result =
            process_packet(parse_state.packet, output, output_size, samples_decoded);
        return result;
    }

    // Demuxer encountered error
#ifndef ESP_PLATFORM
    const char* error_msg = "Unknown error";
    switch (parse_state.result) {
        case micro_ogg::OGG_INVALID_CAPTURE:
            error_msg = "Invalid Ogg capture pattern";
            break;
        case micro_ogg::OGG_INVALID_VERSION:
            error_msg = "Unsupported Ogg version";
            break;
        case micro_ogg::OGG_CRC_FAILED:
            error_msg = "CRC checksum validation failed";
            break;
        case micro_ogg::OGG_STREAM_SEQUENCE_ERROR:
            error_msg = "Page sequence number mismatch";
            break;
        case micro_ogg::OGG_STREAM_BOS_ERROR:
            error_msg = "BOS flag violation (invalid placement)";
            break;
        case micro_ogg::OGG_STREAM_EOS_ERROR:
            error_msg = "EOS flag violation (EOS with continued packet)";
            break;
        case micro_ogg::OGG_STREAM_SERIAL_MISMATCH:
            error_msg = "Stream serial mismatch (concatenated stream)";
            break;
        case micro_ogg::OGG_ALLOCATION_FAILED:
            error_msg = "Memory allocation failed";
            break;
        default:
            break;
    }
    fprintf(stderr, "OggOpusDecoder: Ogg demuxer error (%d): %s\n", parse_state.result, error_msg);
#endif
    return OGG_OPUS_INPUT_INVALID;
}

#ifdef MICRO_OGG_DEMUXER_DEBUG
void OggOpusDecoder::get_demuxer_stats(size_t& zero_copy_count, size_t& buffered_count) const {
    if (ogg_demuxer_) {
        ogg_demuxer_->getStats(zero_copy_count, buffered_count);
    } else {
        zero_copy_count = 0;
        buffered_count = 0;
    }
}

void OggOpusDecoder::get_buffer_stats(size_t& current_capacity, size_t& max_capacity) const {
    if (ogg_demuxer_) {
        ogg_demuxer_->getBufferStats(current_capacity, max_capacity);
    } else {
        current_capacity = 0;
        max_capacity = 0;
    }
}
#endif  // MICRO_OGG_DEMUXER_DEBUG

}  // namespace micro_opus
