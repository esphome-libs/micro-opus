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
 * A C++ wrapper around libopus that handles Ogg container parsing
 * and streaming decoding of Opus audio.
 */

#ifndef OGG_OPUS_DECODER_H
#define OGG_OPUS_DECODER_H

#include <stddef.h>
#include <stdint.h>

#include <memory>

// Forward declarations to avoid exposing implementation details
struct OpusDecoder;
struct OpusMSDecoder;

// Forward declaration from micro_ogg namespace
namespace micro_ogg {
class OggDemuxer;
struct OggPacket;
}  // namespace micro_ogg

namespace micro_opus {

// Opus audio constants
constexpr uint32_t OPUS_DEFAULT_SAMPLE_RATE = 48000;  // Opus always decodes at 48kHz

// Forward declarations
struct OpusHead;

/**
 * @brief Result codes for OggOpusDecoder operations
 *
 * @note Error Checking Pattern:
 *       - Use `result != 0` to check for errors (standard C convention)
 *       - Use `result == 0` (or `!result`) to check for success
 *       - Use `samples_decoded > 0` to check if samples were decoded
 *       - Use `samples_decoded == 0` to check if more input data is needed
 *
 * Success code: OGG_OPUS_OK (0)
 * Error codes: All negative values (< 0)
 */
enum OggOpusResult : int8_t {
    // Success code
    OGG_OPUS_OK = 0,  ///< Success (check samples_decoded output parameter)

    // Input/Stream errors (invalid Ogg container or stream structure)
    OGG_OPUS_INPUT_INVALID = -1,  ///< Invalid Ogg/Opus stream structure

    // Decoder state errors (initialization issues)
    OGG_OPUS_NOT_INITIALIZED = -2,  ///< Decoder not initialized

    // Resource errors (memory and buffer issues)
    OGG_OPUS_ALLOCATION_FAILED = -4,        ///< Memory allocation failed
    OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL = -5,  ///< Output buffer too small for decoded samples

    // Opus decode errors (issues from the Opus decoder itself)
    OGG_OPUS_DECODE_ERROR = -6  ///< Opus decode failed (corrupted/invalid packet)
};

/**
 * @brief Streaming Ogg Opus Decoder
 *
 * This class provides a high-level interface for decoding Opus audio
 * from Ogg container streams. It handles:
 * - Ogg container parsing (pages, packets, segments)
 * - OpusHead and OpusTags header parsing
 * - Streaming decode with user-managed buffers
 * - Minimal internal buffering: only when packets span pages or input is incomplete
 *
 * @warning Thread Safety: This class is NOT thread-safe. Each decoder instance
 *          must be accessed from only one thread at a time. If you need to decode
 *          multiple streams concurrently, create separate decoder instances for
 *          each thread. Do not share a single decoder instance between multiple
 *          threads without external synchronization.
 *
 * @note Lazy Allocation: The constructor always succeeds and does not allocate
 *       any resources. All allocations (OggDemuxer buffers, OpusHead, and Opus
 *       decoder instance) are deferred until the first call to decode().
 *
 *       **First decode() call**: May return OGG_OPUS_ALLOCATION_FAILED if memory
 *       allocation fails (PSRAM or internal RAM). If this occurs, the decoder
 *       remains in an uninitialized state, and subsequent calls will retry
 *       allocation.
 *
 *       **Subsequent calls**: Once allocation succeeds, decode() will never
 *       return OGG_OPUS_ALLOCATION_FAILED again unless reset() is called.
 *
 * Usage:
 * 1. Create decoder instance (constructor always succeeds)
 * 2. Call decode() with chunks of Ogg Opus data
 * 3. Check return value using standard C convention: result != 0 means error
 * 4. Check samples_decoded to see if you got audio samples
 * 5. Advance input pointer by bytes_consumed
 * 6. Repeat until stream is complete
 *
 * Example:
 * @code
 * OggOpusDecoder decoder;  // Constructor always succeeds
 * int16_t pcm_buffer[960 * 2];  // 20ms stereo @ 48kHz
 *
 * while (have_data) {
 *     size_t consumed, samples;
 *     OggOpusResult result = decoder.decode(
 *         input_ptr, input_len,
 *         pcm_buffer, sizeof(pcm_buffer),
 *         consumed, samples
 *     );
 *
 *     // Standard C error checking
 *     if (result != 0) {
 *         // Handle error (allocation failure, invalid stream, etc.)
 *         break;
 *     }
 *
 *     // Success! Check if we got samples
 *     if (samples > 0) {
 *         // Process PCM samples (per channel)
 *         process_audio(pcm_buffer, samples);
 *     }
 *     // else: samples == 0 means need more input data
 *
 *     input_ptr += consumed;
 *     input_len -= consumed;
 * }
 * @endcode
 */
class OggOpusDecoder {
public:
    /**
     * @brief Construct a new Ogg Opus Decoder
     *
     * The constructor always succeeds and does not allocate any resources.
     * All allocations are deferred to the first call to decode().
     *
     * @param enable_crc Enable CRC32 validation of Ogg pages (default false)
     * @param sample_rate Output sample rate in Hz. Must be one of: 8000, 12000,
     *                    16000, 24000, 48000. Default is 48000 (native Opus rate).
     *                    Lower rates reduce CPU usage but lose high-frequency content.
     * @param channels Output channel count. 0 = use file's channel count (default).
     *                 1 = mono, 2 = stereo. The Opus decoder handles mixing/duplication.
     *
     * @note This constructor is guaranteed not to fail. Resource allocation
     *       is deferred to the first decode() call, which can return
     *       OGG_OPUS_ALLOCATION_FAILED if memory allocation fails.
     *
     * @note CRC Validation: When enabled, all Ogg pages are validated
     *       using CRC32 checksums as recommended by RFC 3533. Disabling CRC
     *       validation can provide a performance improvement but sacrifices
     *       data integrity checking. Only disable CRC for trusted sources (local
     *       files) or when performance is critical and corruption detection is
     *       not required.
     */
    OggOpusDecoder(bool enable_crc = false, uint32_t sample_rate = OPUS_DEFAULT_SAMPLE_RATE,
                   uint8_t channels = 0);

    /**
     * @brief Destroy the decoder and free resources
     */
    ~OggOpusDecoder();

    /**
     * @brief Decode Ogg Opus data and output PCM samples
     *
     * This method processes input data, parsing Ogg pages and packets,
     * and decoding Opus frames to PCM output.
     *
     * @param input Pointer to input Ogg Opus data (must not be nullptr)
     * @param input_len Number of bytes available in input
     * @param output Pointer to output buffer for PCM samples (must not be nullptr)
     * @param output_size Number of bytes available in output buffer
     * @param bytes_consumed [OUT] Number of input bytes consumed (may be buffered internally)
     * @param samples_decoded [OUT] Number of PCM samples decoded (per channel)
     *
     * @return OggOpusResult result code
     *         - 0 (OGG_OPUS_OK): Success (check samples_decoded to see if you got samples)
     *         - Negative values: Error occurred
     *           - OGG_OPUS_ALLOCATION_FAILED: Memory allocation failed (first call only)
     *           - OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL: Output buffer too small
     *           - OGG_OPUS_INPUT_INVALID: Invalid stream
     *           - OGG_OPUS_DECODE_*: Opus decode errors (see OggOpusResult enum)
     *
     * @note **Lazy Allocation**: On the first call, this method allocates internal
     *       resources (~128KB, preferring PSRAM on ESP32). If allocation fails,
     *       returns OGG_OPUS_ALLOCATION_FAILED and decoder remains uninitialized.
     *       Subsequent calls will retry allocation until it succeeds.
     *
     * @note **Parameter Validation**: Both input and output pointers must be valid
     *       (non-null). Passing nullptr for either parameter will return
     *       OGG_OPUS_INPUT_INVALID, even if input_len is 0.
     *
     * @note The user must advance the input pointer by bytes_consumed before
     *       calling decode() again.
     * @note output_size is in bytes. For stereo 16-bit audio, you need
     *       output_size >= samples_per_frame * 2 channels * 2 bytes.
     * @note Can handle arbitrarily small input chunks (even 1 byte at a time)
     *       thanks to internal header staging buffer.
     */
    OggOpusResult decode(const uint8_t* input, size_t input_len, int16_t* output,
                         size_t output_size, size_t& bytes_consumed, size_t& samples_decoded);

    /**
     * @brief Get the sample rate of the decoded audio
     *
     * @return Sample rate in Hz (48000, 24000, 16000, 12000, or 8000)
     *         Returns 0 if header not yet parsed
     *
     * @note This is the decoder's sample rate, not the original input
     *       sample rate (which is stored in OpusHead for informational purposes)
     */
    uint32_t get_sample_rate() const;

    /**
     * @brief Get the number of channels
     *
     * @return Number of channels (1 for mono, 2 for stereo, etc.)
     *         Returns 0 if header not yet parsed
     */
    uint8_t get_channels() const;

    /**
     * @brief Get the bit depth of decoded samples
     *
     * @return Bit depth (always 16 for int16_t output samples)
     */
    uint8_t get_bit_depth() const;

    /**
     * @brief Get the number of bytes per sample
     *
     * @return Bytes per sample (always 2 for int16_t output samples)
     */
    uint8_t get_bytes_per_sample() const;

    /**
     * @brief Get the pre-skip value
     *
     * Pre-skip is the number of samples (at 48kHz) that should be
     * discarded from the beginning of the stream.
     *
     * @return Pre-skip samples at 48kHz, or 0 if header not yet parsed
     */
    uint16_t get_pre_skip() const;

    /**
     * @brief Get the output gain
     *
     * Output gain in Q7.8 dB units. Divide by 256.0 to get dB.
     *
     * @return Output gain, or 0 if header not yet parsed
     */
    int16_t get_output_gain() const;

    /**
     * @brief Get the required output buffer size for the last packet
     *
     * This method returns the buffer size (in bytes) needed to decode
     * the most recently processed audio packet. It is particularly useful
     * after receiving OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL to determine the
     * correct buffer size to allocate.
     *
     * The returned value accounts for:
     * - Number of samples in the packet (based on frame size and frame count)
     * - Number of output channels
     * - Sample size (sizeof(int16_t) = 2 bytes)
     *
     * @return Required buffer size in bytes, or 0 if no audio packet has been
     *         processed yet (i.e., still parsing headers)
     *
     * @note This value is updated each time an audio packet is processed,
     *       regardless of whether decoding succeeded or failed.
     */
    size_t get_required_output_buffer_size() const;

    /**
     * @brief Check if the OpusHead header has been parsed
     *
     * @return true if header is parsed and decoder is initialized
     */
    bool is_initialized() const;

    /**
     * @brief Reset the decoder state
     *
     * Resets all internal state, allowing the decoder to be reused
     * for a new stream. This does NOT deallocate internal buffers -
     * they are preserved for reuse. After calling reset(), the next
     * decode() call will NOT return OGG_OPUS_ALLOCATION_FAILED unless
     * the decoder was never successfully initialized.
     *
     * @note Preserves allocated buffers for efficiency. To fully release
     *       memory, destroy the decoder instance.
     */
    void reset();

#ifdef MICRO_OGG_DEMUXER_DEBUG
    /**
     * @brief Get debug state from demuxer (for debugging only)
     */
    void get_demuxer_debug_state(int& state, bool& assembling, bool& skipping, size_t& packet_size,
                                 size_t& body_consumed, uint8_t& seg_index,
                                 uint8_t& seg_count) const;

    /**
     * @brief Get zero-copy statistics from the internal OggDemuxer
     *
     * @param zero_copy_count Output: number of packets returned via zero-copy
     * @param buffered_count Output: number of packets that required buffering
     *
     * @note These statistics track all packets demuxed, including headers (OpusHead/OpusTags)
     */
    void get_demuxer_stats(size_t& zero_copy_count, size_t& buffered_count) const;

    /**
     * @brief Get buffer statistics from the internal OggDemuxer
     *
     * @param current_capacity Output: current internal buffer capacity in bytes
     * @param max_capacity Output: maximum internal buffer capacity reached in bytes
     */
    void get_buffer_stats(size_t& current_capacity, size_t& max_capacity) const;
#endif  // MICRO_OGG_DEMUXER_DEBUG

private:
    // Disable copy and assignment
    OggOpusDecoder(const OggOpusDecoder&) = delete;
    OggOpusDecoder& operator=(const OggOpusDecoder&) = delete;

    // Internal packet processing
    OggOpusResult process_packet(const micro_ogg::OggPacket& packet, int16_t* output,
                                 size_t output_size, size_t& samples_decoded);

    // Page boundary tracking helper
    void update_page_tracking(bool is_last_on_page);

    // Granule position validation helper (RFC 7845 compliance)
    OggOpusResult validate_granule_position(int64_t granule_pos, size_t decoded_samples,
                                            bool is_eos, bool is_last_on_page);

    // Pre-skip handling helper
    OggOpusResult apply_pre_skip(int16_t* output, size_t decoded_samples, uint8_t output_channels,
                                 size_t& samples_decoded);

    // Opus decoder creation helper
    OggOpusResult create_opus_decoder(uint8_t output_channels);

    // State handlers
    OggOpusResult handle_opus_head_packet(const uint8_t* packet_data, size_t packet_len,
                                          int64_t granule_pos, bool is_bos, bool is_last_on_page);
    OggOpusResult handle_opus_tags_packet(const uint8_t* packet_data, size_t packet_len,
                                          int64_t granule_pos, bool is_last_on_page);
    OggOpusResult handle_audio_packet(const uint8_t* packet_data, size_t packet_len,
                                      int64_t granule_pos, bool is_eos, bool is_last_on_page,
                                      int16_t* output, size_t output_size, size_t& samples_decoded);

    // Internal state machine
    enum State : uint8_t { STATE_EXPECT_OPUS_HEAD, STATE_EXPECT_OPUS_TAGS, STATE_DECODING };

    // =======================================================================
    // Member variables ordered by size (largest to smallest) to minimize padding
    // =======================================================================

    // --- Pointer-sized members (8 bytes on 64-bit) ---

    // Ogg demuxer
    std::unique_ptr<micro_ogg::OggDemuxer> ogg_demuxer_;

    // Opus header info
    std::unique_ptr<OpusHead> opus_head_;

    // Opus decoder instances (C API handles - managed with create/destroy)
    // Only one of these will be non-null at a time:
    // - opus_decoder_ for channel_mapping == 0 (mono/stereo)
    // - opus_ms_decoder_ for channel_mapping != 0 (multistream with channel mapping)
    OpusDecoder* opus_decoder_{nullptr};
    OpusMSDecoder* opus_ms_decoder_{nullptr};

    // --- 64-bit members ---

    // Pre-skip tracking
    uint64_t samples_decoded_total_{0};

    // Granule position tracking for validation (RFC 7845 Section 4)
    int64_t last_granule_position_{0};

    // RFC 7845 Section 4: Total size across all continuation pages
    size_t opus_tags_accumulated_size_{0};

    // Required output buffer size for the last audio packet (in bytes)
    size_t last_required_buffer_bytes_{0};

    // RFC 7845 Section 4: First audio data page granule position validation
    // Tracks total samples that complete on the first audio data page
    // -1 = not yet on first audio page, 0+ = accumulating samples, validated after first page
    // completes
    int64_t first_audio_page_samples_{-1};

    // --- 32-bit members ---

    State state_{STATE_EXPECT_OPUS_HEAD};

    // Decoder parameters
    uint32_t sample_rate_{OPUS_DEFAULT_SAMPLE_RATE};

    // --- 8-bit / bool members ---

    // Ogg demuxer configuration
    bool enable_crc_;  // CRC validation setting (passed to OggDemuxer)

    // Output channel count (0 = use file's channel count)
    uint8_t channels_{0};

    // Resolved output channel count (set after OpusHead parsing)
    uint8_t output_channels_{0};

    // Pre-skip tracking
    bool pre_skip_applied_{false};

    // RFC 7845 Section 4: Track header packets
    bool has_seen_opus_head_{false};
    bool has_seen_opus_tags_{false};

    // RFC 7845 Section 4: Track packets per page for isolation validation
    uint8_t packets_on_current_page_{0};

    // RFC 7845 Section 3: End of stream validation
    // "There MUST NOT be any more pages in an Opus logical bitstream after a page marked 'end of
    // stream'."
    bool eos_seen_{false};

    // RFC 7845 Section 4.1: Track continued packet flag for validation
    // "If a page has the 'continued packet' flag set and the previous page with packet data
    // does not end in a continued packet (does not end with a lacing value of 255), then a
    // demuxer MUST NOT attempt to decode the data for the first packet on the page."
    bool expect_continued_packet_{false};
    bool previous_packet_was_last_on_page_{true};  // Track page boundaries for validation
};

}  // namespace micro_opus

#endif  // OGG_OPUS_DECODER_H
