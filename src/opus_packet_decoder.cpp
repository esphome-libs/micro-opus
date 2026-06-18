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

/* Raw Opus Packet Decoder
 * Implementation of OpusPacketDecoder class
 */

#include "micro_opus/opus_packet_decoder.h"

#include "opus.h"

#include <algorithm>
#include <climits>

namespace micro_opus {

// ============================================================================
// Lifecycle
// ============================================================================

OpusPacketDecoder::OpusPacketDecoder(uint32_t sample_rate, uint8_t channels) {
    this->pcm_format_.sample_rate_ = sample_rate;
    this->pcm_format_.num_channels_ = channels;
}

OpusPacketDecoder::~OpusPacketDecoder() {
    if (this->opus_decoder_ != nullptr) {
        opus_decoder_destroy(this->opus_decoder_);
        this->opus_decoder_ = nullptr;
    }
}

void OpusPacketDecoder::reset() {
    if (this->opus_decoder_ != nullptr) {
        opus_decoder_ctl(this->opus_decoder_, OPUS_RESET_STATE);
    }
    this->required_output_bytes_ = 0;
}

// ============================================================================
// Configuration
// ============================================================================

void OpusPacketDecoder::set_output_gain(int16_t output_gain) {
    this->output_gain_ = output_gain;
    // Apply now if the decoder already exists; otherwise ensure_decoder() applies it on creation.
    if (this->opus_decoder_ != nullptr) {
        opus_decoder_ctl(this->opus_decoder_, OPUS_SET_GAIN(static_cast<opus_int32>(output_gain)));
    }
}

// ============================================================================
// Core Decoding API
// ============================================================================

OpusPacketResult OpusPacketDecoder::decode(const uint8_t* input, size_t input_len, uint8_t* output,
                                           size_t output_size_bytes, size_t& bytes_written) {
    bytes_written = 0;

    if (input == nullptr || input_len == 0 || output == nullptr) {
        return OPUS_PACKET_DECODER_ERROR_INPUT_INVALID;
    }

    OpusPacketResult init_result = this->ensure_decoder();
    if (init_result < 0) {
        return init_result;
    }

    const size_t bytes_per_frame = this->pcm_format_.num_channels() * sizeof(int16_t);

    // An invalid packet makes opus_packet_get_nb_samples() return < 0; skip the up-front size
    // check then and let opus_decode() report the specific failure.
    int nb_samples =
        opus_packet_get_nb_samples(input, static_cast<opus_int32>(input_len),
                                   static_cast<opus_int32>(this->pcm_format_.sample_rate()));
    if (nb_samples > 0) {
        this->required_output_bytes_ = static_cast<size_t>(nb_samples) * bytes_per_frame;
        if (output_size_bytes < this->required_output_bytes_) {
            return OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL;
        }
    }

    int max_frame_size = static_cast<int>(
        std::min(output_size_bytes / bytes_per_frame, static_cast<size_t>(INT_MAX)));

    int decoded = opus_decode(this->opus_decoder_, input, static_cast<opus_int32>(input_len),
                              reinterpret_cast<int16_t*>(output), max_frame_size, 0);
    if (decoded < 0) {
        return (decoded == OPUS_BUFFER_TOO_SMALL)
                   ? OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL
                   : OPUS_PACKET_DECODER_ERROR_DECODE_FAILED;
    }

    bytes_written = static_cast<size_t>(decoded) * bytes_per_frame;
    return OPUS_PACKET_DECODER_SUCCESS;
}

OpusPacketResult OpusPacketDecoder::conceal_loss(uint8_t* output, size_t output_size_bytes,
                                                 size_t frame_size_samples, size_t& bytes_written) {
    bytes_written = 0;

    if (output == nullptr || frame_size_samples == 0) {
        return OPUS_PACKET_DECODER_ERROR_INPUT_INVALID;
    }

    OpusPacketResult init_result = this->ensure_decoder();
    if (init_result < 0) {
        return init_result;
    }

    const size_t bytes_per_frame = this->pcm_format_.num_channels() * sizeof(int16_t);

    this->required_output_bytes_ = frame_size_samples * bytes_per_frame;
    if (output_size_bytes < this->required_output_bytes_) {
        return OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL;
    }

    // A null packet asks libopus to synthesize one frame of concealment audio from recent history.
    int decoded = opus_decode(this->opus_decoder_, nullptr, 0, reinterpret_cast<int16_t*>(output),
                              static_cast<int>(frame_size_samples), 0);
    if (decoded < 0) {
        return (decoded == OPUS_BUFFER_TOO_SMALL)
                   ? OPUS_PACKET_DECODER_ERROR_OUTPUT_BUFFER_TOO_SMALL
                   : OPUS_PACKET_DECODER_ERROR_DECODE_FAILED;
    }

    bytes_written = static_cast<size_t>(decoded) * bytes_per_frame;
    return OPUS_PACKET_DECODER_SUCCESS;
}

// ============================================================================
// Decode Pipeline
// ============================================================================

OpusPacketResult OpusPacketDecoder::ensure_decoder() {
    if (this->opus_decoder_ != nullptr) {
        return OPUS_PACKET_DECODER_SUCCESS;
    }

    int error = 0;
    this->opus_decoder_ =
        opus_decoder_create(static_cast<opus_int32>(this->pcm_format_.sample_rate()),
                            static_cast<int>(this->pcm_format_.num_channels()), &error);
    if (this->opus_decoder_ == nullptr) {
        // OPUS_BAD_ARG means an unsupported sample rate or channel count was given to the
        // constructor; anything else (e.g. OPUS_ALLOC_FAIL) is an out-of-memory condition.
        return (error == OPUS_BAD_ARG) ? OPUS_PACKET_DECODER_ERROR_INPUT_INVALID
                                       : OPUS_PACKET_DECODER_ERROR_ALLOCATION_FAILED;
    }

    // Apply any gain set before allocation (e.g. a forwarded OpusHead output_gain).
    // Unity gain (0) is the libopus default, so skip the ctl.
    if (this->output_gain_ != 0) {
        opus_decoder_ctl(this->opus_decoder_,
                         OPUS_SET_GAIN(static_cast<opus_int32>(this->output_gain_)));
    }
    return OPUS_PACKET_DECODER_SUCCESS;
}

}  // namespace micro_opus
