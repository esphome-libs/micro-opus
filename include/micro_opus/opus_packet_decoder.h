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

/// @file opus_packet_decoder.h
/// @brief Raw Opus packet decoder (no Ogg container, no OpusHead)

#pragma once

#include <cstddef>
#include <cstdint>

// Forward declaration of the libopus C decoder handle to avoid exposing opus.h.
struct OpusDecoder;

namespace micro_opus {

// ============================================================================
// Public Types
// ============================================================================

/// @brief Result codes for OpusPacketDecoder operations
///
/// Non-negative values (>= 0) indicate success, negative values indicate errors. One error,
/// OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL, is recoverable: size the output buffer with
/// get_required_output_bytes() and retry the same call. Any other error leaves the decoder usable
/// for the next packet but discards the current one.
///
/// This is a pre-framed decoder: the output format is supplied at construction (so there is no
/// "format ready" event), each call consumes exactly one complete packet (so input is never
/// "incomplete"), and there is no container to signal end-of-stream. It therefore returns only
/// OPUS_PACKET_DECODER_SUCCESS or an error; the canonical streaming/informational codes do not
/// apply.
///
/// Error checking pattern:
/// - Use `result < 0` to check for errors
/// - Use `result == OPUS_PACKET_DECODER_SUCCESS` to check for success (then read bytes_written)
enum OpusPacketResult : int8_t {
    // Success / informational (>= 0)
    OPUS_PACKET_DECODER_SUCCESS = 0,  // Packet decoded (check bytes_written output parameter)

    // Errors (< 0)
    OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL =
        -1,  // Output buffer can't hold this packet; size it with get_required_output_bytes()
    OPUS_PACKET_DECODER_ERROR_INPUT_INVALID = -2,  // Null/empty packet or invalid arguments
    OPUS_PACKET_DECODER_ERROR_ALLOCATION_FAILED =
        -3,                                       // Decoder state allocation failed (first use)
    OPUS_PACKET_DECODER_ERROR_DECODE_FAILED = -4  // libopus rejected the packet (corrupt/invalid)
};

/// @brief Format of the PCM that decode() produces
///
/// Describes the decoder's output, not a source file: a raw Opus stream carries no OpusHead, so the
/// sample rate and channel count come from the constructor. The format is therefore valid
/// immediately after construction (is_valid() is true), before the first decode() call. Bit depth
/// is fixed because the decoder always emits 16-bit signed PCM.
class PcmFormat {
    friend class OpusPacketDecoder;

    // 32-bit fields
    uint32_t sample_rate_{0};  // Output sample rate in Hz (from the constructor)

    // 8-bit fields
    uint8_t num_channels_{0};  // Output channel count (from the constructor)

public:
    /// @brief Bits per output sample (always 16; microOpus emits 16-bit signed PCM)
    /// @return Output bit depth in bits (always 16)
    uint32_t bits_per_sample() const {
        return 16;
    }
    /// @brief Bytes per output sample (always 2; microOpus emits 16-bit signed PCM)
    /// @return Output bytes per sample (always 2)
    uint32_t bytes_per_sample() const {
        return 2;
    }
    /// @brief Safe output buffer size, in bytes, for any single decode() call
    ///
    /// Sized for the Opus worst case: a 120 ms packet, which at the output rate is
    /// sample_rate / 1000 * 120 samples per channel. Allocate this many bytes and decode() never
    /// returns OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL; pass it directly as decode()'s
    /// output_size_bytes.
    /// @return Output buffer size in bytes (all channels)
    uint32_t max_output_bytes() const {
        return (this->sample_rate_ / 1000u * 120u) * this->num_channels_ * this->bytes_per_sample();
    }
    /// @brief Number of output channels (1 = mono, 2 = stereo)
    /// @return Output channel count
    uint32_t num_channels() const {
        return this->num_channels_;
    }
    /// @brief Output sample rate in Hz (8000, 12000, 16000, 24000, or 48000)
    /// @return Sample rate in Hz
    uint32_t sample_rate() const {
        return this->sample_rate_;
    }
    /// @brief Whether the stream's PCM format is known
    ///
    /// Always true for this decoder: the format is supplied at construction, so it is known as soon
    /// as the object exists.
    /// @return true (the format is always available once constructed)
    bool is_valid() const {
        return true;
    }
};

// ============================================================================
// OpusPacketDecoder
// ============================================================================

/**
 * @brief Raw Opus packet decoder (no Ogg, no OpusHead)
 *
 * Decodes individual, complete Opus packets without any container framing. Each call to decode()
 * must be handed exactly one whole packet (as delivered by a transport that already frames them,
 * e.g. an RTP payload or a length-prefixed application stream). The stream carries no OpusHead, so
 * the caller supplies the output sample rate and channel count at construction.
 *
 * Use this when an upstream layer guarantees packet boundaries. For Ogg-contained Opus files use
 * OggOpusDecoder instead, which parses the container and OpusHead for you.
 *
 * @warning Thread Safety: This class is NOT thread-safe. Each decoder instance must be accessed
 * from only one thread at a time. To decode multiple streams concurrently, create one decoder per
 * thread.
 *
 * @note Lazy Allocation: The constructor always succeeds and does not allocate. The libopus decoder
 *       state is allocated on the first decode() (or conceal_loss()) call, preferring PSRAM on
 * ESP32 per the OPUS_STATE_MEMORY_PREFERENCE Kconfig. If allocation fails, that call returns
 *       OPUS_PACKET_DECODER_ERROR_ALLOCATION_FAILED and the decoder stays uninitialized; later
 * calls retry. Once allocation succeeds it does not recur.
 *
 * @note Channels: mono and stereo only (Opus channel mapping family 0). A raw stream provides no
 *       channel mapping table, which multistream (> 2 channels) decoding would require.
 *
 * Usage:
 * 1. Construct with the stream's sample rate and channel count (constructor always succeeds)
 * 2. Size an output buffer from get_pcm_format().max_output_bytes() (or grow lazily; see below)
 * 3. Call decode() with one complete Opus packet per call
 * 4. Check the return value: result < 0 means error; OPUS_PACKET_DECODER_SUCCESS yields
 * bytes_written
 *
 * Example:
 * @code
 * micro_opus::OpusPacketDecoder decoder(48000, 2);  // 48 kHz stereo
 *
 * const auto& fmt = decoder.get_pcm_format();
 * // int16_t buffer: naturally aligned for the decoder's 16-bit PCM output. The API stays byte-
 * // oriented, so cast to uint8_t* and pass the size in bytes at the call site.
 * std::vector<int16_t> pcm(fmt.max_output_bytes() / sizeof(int16_t));
 *
 * for (const Packet& p : packets) {
 *     size_t bytes_written = 0;
 *     auto result = decoder.decode(p.data, p.size, reinterpret_cast<uint8_t*>(pcm.data()),
 *                                  pcm.size() * sizeof(int16_t), bytes_written);
 *     if (result < 0) {
 *         break;  // With the buffer sized above, OUTPUT_BUFFER_TOO_SMALL never fires
 *     }
 *     // bytes_written is the total across all channels; e.g. a 20 ms stereo packet at 48 kHz
 *     // is 960 frames => 1920 samples => 3840 bytes.
 *     process_audio(pcm.data(), bytes_written / sizeof(int16_t));
 * }
 * @endcode
 */
class OpusPacketDecoder {
public:
    // ========================================
    // Lifecycle
    // ========================================

    /// @brief Construct a raw Opus packet decoder
    ///
    /// The constructor always succeeds and does not allocate; the libopus decoder state is created
    /// lazily on the first decode() call. The PCM format is populated immediately, so
    /// get_pcm_format() is usable before decoding.
    ///
    /// @param sample_rate Output sample rate in Hz. Must be one of 8000, 12000, 16000, 24000, or
    ///                    48000 (the rates Opus can decode to); other values are rejected on the
    ///                    first decode() call. Default 48000 (native Opus rate).
    /// @param channels Output channel count: 1 (mono) or 2 (stereo). Default 2.
    explicit OpusPacketDecoder(uint32_t sample_rate = 48000, uint8_t channels = 2);

    /// @brief Destroy the decoder and free the libopus decoder state
    ~OpusPacketDecoder();

    // Non-copyable, non-movable: owns a libopus decoder handle (a fixed-in-place resource).
    OpusPacketDecoder(const OpusPacketDecoder&) = delete;
    OpusPacketDecoder& operator=(const OpusPacketDecoder&) = delete;
    OpusPacketDecoder(OpusPacketDecoder&&) = delete;
    OpusPacketDecoder& operator=(OpusPacketDecoder&&) = delete;

    /// @brief Reset the decoder's inter-packet state, ready for a new stream
    ///
    /// Clears the libopus decoder's internal history (the state carried between consecutive
    /// packets) and the required-output-bytes counter. The configured sample rate and channel
    /// count, and the allocated decoder buffer, are preserved, so the next decode() does not
    /// re-allocate or return OPUS_PACKET_DECODER_ERROR_ALLOCATION_FAILED (unless the decoder was
    /// never initialized). To fully release memory, destroy the instance.
    void reset();

    // ========================================
    // Configuration
    // ========================================

    /// @brief Apply a fixed output gain to all decoded audio (Ogg Opus OpusHead "output gain")
    ///
    /// Sets a gain that libopus applies to every decoded frame, matching the OpusHead output_gain
    /// field (Q7.8 dB: the stored value divided by 256 gives dB). A raw Opus stream carries no such
    /// field, so it defaults to 0 (unity gain); it exists so a container parser like OggOpusDecoder
    /// can forward the header value.
    ///
    /// Takes effect on the next decoder allocation, or immediately if the decoder already exists.
    /// The gain is preserved across reset() (libopus keeps it through OPUS_RESET_STATE).
    ///
    /// @param output_gain Output gain in Q7.8 dB units (0 = unity gain)
    void set_output_gain(int16_t output_gain);

    // ========================================
    // Core Decoding API
    // ========================================

    /// @brief Decode one complete Opus packet to PCM
    ///
    /// Each call must provide exactly one whole Opus packet. The decoder writes 16-bit signed PCM,
    /// interleaved in channel order.
    ///
    /// @param input Pointer to the Opus packet (must not be nullptr)
    /// @param input_len Number of bytes in the packet (must not be 0)
    /// @param output Pointer to the output buffer (must not be nullptr). Must be aligned for
    /// int16_t
    ///               access (2-byte alignment); buffers from new/malloc/heap_caps_malloc satisfy
    ///               this.
    /// @param output_size_bytes Number of bytes available in the output buffer
    /// @param[out] bytes_written Number of PCM bytes written (total across all channels; e.g. a
    ///                           stereo packet of 960 frames => 1920 samples => 3840 bytes). Set to
    ///                           0 on any error.
    ///
    /// @return OPUS_PACKET_DECODER_SUCCESS, or a negative error code; see OpusPacketResult
    ///
    /// @note On OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL, call get_required_output_bytes()
    ///       for the exact size this packet needs, grow the buffer, and retry the same call.
    /// @note output_size_bytes and bytes_written are both in bytes, matching the output buffer's
    /// unit.
    OpusPacketResult decode(const uint8_t* input, size_t input_len, uint8_t* output,
                            size_t output_size_bytes, size_t& bytes_written);

    /// @brief Synthesize concealment audio for a lost packet (packet-loss concealment)
    ///
    /// When a packet is known to be lost, call this in its place to let libopus extrapolate one
    /// frame of audio from the decoder's recent history. The decoder state advances as if the lost
    /// packet had been decoded, keeping later packets aligned.
    ///
    /// @param output Pointer to the output buffer (must not be nullptr), aligned for int16_t access
    /// @param output_size_bytes Number of bytes available in the output buffer
    /// @param frame_size_samples Number of samples per channel to synthesize. Must be a valid Opus
    ///                           frame size at the configured rate (a multiple of 2.5 ms, e.g. 960
    ///                           for 20 ms at 48 kHz); typically the frame size of the surrounding
    ///                           packets. Must not be 0.
    /// @param[out] bytes_written Number of PCM bytes written (total across all channels). Set to 0
    /// on
    ///                           any error.
    ///
    /// @return OPUS_PACKET_DECODER_SUCCESS, or a negative error code; see OpusPacketResult
    OpusPacketResult conceal_loss(uint8_t* output, size_t output_size_bytes,
                                  size_t frame_size_samples, size_t& bytes_written);

    // ========================================
    // PCM Format
    // ========================================

    /// @brief Get the format of the PCM that decode() produces
    ///
    /// Valid immediately after construction (the format comes from the constructor, not a header).
    ///
    /// @return Reference to the PcmFormat struct
    const PcmFormat& get_pcm_format() const {
        return this->pcm_format_;
    }

    // ========================================
    // Output Buffer Helpers
    // ========================================

    /// @brief Get the required output buffer size, in bytes, for the last packet
    ///
    /// Returns the byte count (across all channels) the most recently processed packet needs. Call
    /// it after OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL to size the buffer before
    /// retrying. Unlike PcmFormat::max_output_bytes() (a stream-wide upper bound), this is the
    /// exact size the last packet needs, so callers that allocate lazily can grow to fit each
    /// packet.
    ///
    /// @return Required size in bytes (all channels), or 0 if no packet has been processed yet
    size_t get_required_output_bytes() const {
        return this->required_output_bytes_;
    }

private:
    // ========================================
    // Decode Pipeline
    // ========================================

    /// @brief Create the libopus decoder state on first use (lazy allocation)
    OpusPacketResult ensure_decoder();

    // ========================================
    // Member Variables
    // ========================================

    // Struct fields

    // Output PCM format (populated by the constructor)
    PcmFormat pcm_format_{};

    // Pointer fields

    // libopus decoder handle (created lazily on first decode; nullptr until then)
    OpusDecoder* opus_decoder_{nullptr};

    // size_t fields

    // Output byte count (all channels) the last packet needs
    size_t required_output_bytes_{0};

    // 16-bit fields

    // Fixed output gain (Q7.8 dB) applied via OPUS_SET_GAIN; 0 = unity. From set_output_gain().
    int16_t output_gain_{0};
};

}  // namespace micro_opus
