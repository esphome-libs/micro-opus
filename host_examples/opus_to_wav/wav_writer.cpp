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

/* Simple WAV File Writer Implementation */

#include "wav_writer.h"

#include <cstring>

// WAV file format constants
constexpr uint32_t WAV_HEADER_SIZE = 36;  // Size of RIFF + fmt + data headers before audio data

// WAV file header structures (little-endian)
#pragma pack(push, 1)
struct RIFFHeader {
    char chunk_id[4];     // "RIFF"
    uint32_t chunk_size;  // File size - 8
    char format[4];       // "WAVE"
};

struct FmtChunk {
    char chunk_id[4];       // "fmt "
    uint32_t chunk_size;    // 16 for PCM
    uint16_t audio_format;  // 1 for PCM
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;    // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align;  // num_channels * bits_per_sample/8
    uint16_t bits_per_sample;
};

struct DataChunk {
    char chunk_id[4];     // "data"
    uint32_t chunk_size;  // Number of bytes in data
};
#pragma pack(pop)

WavWriter::WavWriter(const std::string& filename, uint32_t sample_rate, uint16_t num_channels,
                     uint16_t bits_per_sample)
    : file_(fopen(filename.c_str(), "wb")),
      sample_rate_(sample_rate),
      num_channels_(num_channels),
      bits_per_sample_(bits_per_sample),
      samples_written_(0) {
    if (file_) {
        writeHeader();
    }
}

WavWriter::~WavWriter() {
    if (file_) {
        updateHeader();
        fclose(file_);
        file_ = nullptr;
    }
}

void WavWriter::writeHeader() {
    RIFFHeader riff{};
    memcpy(riff.chunk_id, "RIFF", 4);
    riff.chunk_size = 0;  // Will update later
    memcpy(riff.format, "WAVE", 4);

    FmtChunk fmt{};
    memcpy(fmt.chunk_id, "fmt ", 4);
    fmt.chunk_size = 16;
    fmt.audio_format = 1;  // PCM
    fmt.num_channels = num_channels_;
    fmt.sample_rate = sample_rate_;
    fmt.byte_rate = sample_rate_ * num_channels_ * (bits_per_sample_ / 8);
    fmt.block_align = num_channels_ * (bits_per_sample_ / 8);
    fmt.bits_per_sample = bits_per_sample_;

    DataChunk data{};
    memcpy(data.chunk_id, "data", 4);
    data.chunk_size = 0;  // Will update later

    fwrite(&riff, sizeof(riff), 1, file_);
    fwrite(&fmt, sizeof(fmt), 1, file_);
    fwrite(&data, sizeof(data), 1, file_);
}

void WavWriter::updateHeader() {
    if (!file_) {
        return;
    }

    uint32_t data_size = samples_written_ * num_channels_ * (bits_per_sample_ / 8);
    uint32_t file_size = data_size + WAV_HEADER_SIZE;

    // Update RIFF chunk size
    fseek(file_, 4, SEEK_SET);
    fwrite(&file_size, 4, 1, file_);

    // Update data chunk size
    fseek(file_, 40, SEEK_SET);
    fwrite(&data_size, 4, 1, file_);

    // Return to end of file
    fseek(file_, 0, SEEK_END);
}

bool WavWriter::writeSamples(const int16_t* samples, size_t num_samples) {
    if (!file_ || !samples || num_samples == 0) {
        return false;
    }

    size_t bytes_to_write = num_samples * num_channels_ * sizeof(int16_t);
    size_t bytes_written = fwrite(samples, 1, bytes_to_write, file_);

    if (bytes_written == bytes_to_write) {
        samples_written_ += num_samples;
        return true;
    }

    return false;
}
