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

/* Simple WAV File Writer
 * Supports PCM audio in standard RIFF/WAVE format
 */

#ifndef WAV_WRITER_HPP
#define WAV_WRITER_HPP

#include <cstdint>
#include <cstdio>
#include <string>

class WavWriter {
public:
    /**
     * @brief Construct a new WAV Writer
     *
     * @param filename Output WAV file path
     * @param sample_rate Sample rate in Hz
     * @param num_channels Number of channels (1=mono, 2=stereo)
     * @param bits_per_sample Bits per sample (typically 16)
     */
    WavWriter(const std::string& filename, uint32_t sample_rate, uint16_t num_channels,
              uint16_t bits_per_sample = 16);

    /**
     * @brief Destroy the WAV Writer and finalize the file
     */
    ~WavWriter();

    /**
     * @brief Write PCM samples to the WAV file
     *
     * @param samples Pointer to PCM samples (int16_t array)
     * @param num_samples Number of samples (per channel)
     * @return true if successful, false on error
     */
    bool writeSamples(const int16_t* samples, size_t num_samples);

    /**
     * @brief Check if the WAV file is open and ready
     *
     * @return true if file is open
     */
    bool isOpen() const {
        return file_ != nullptr;
    }

    /**
     * @brief Get the total number of samples written
     *
     * @return Total samples written (per channel)
     */
    uint64_t getSamplesWritten() const {
        return samples_written_;
    }

private:
    // Disable copy and assignment
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;

    void writeHeader();
    void updateHeader();

    FILE* file_;
    uint32_t sample_rate_;
    uint16_t num_channels_;
    uint16_t bits_per_sample_;
    uint64_t samples_written_;
};

#endif  // WAV_WRITER_HPP
