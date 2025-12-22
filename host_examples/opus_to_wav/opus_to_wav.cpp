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

/* Ogg Opus to WAV Converter
 * Converts .opus files to .wav format using microOpus
 */

#include "micro_opus/ogg_opus_decoder.h"
#include "wav_writer.h"

#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <vector>

void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " <input.opus> <output.wav>\n";
    std::cerr << "\nConverts an Ogg Opus file to WAV format.\n";
}

void print_error_description(micro_opus::OggOpusResult result) {
    switch (result) {
        case micro_opus::OGG_OPUS_INPUT_INVALID:
            std::cerr << " (OGG_OPUS_INPUT_INVALID - Invalid Ogg/Opus stream)";
            break;
        case micro_opus::OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL:
            std::cerr << " (OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL)";
            break;
        case micro_opus::OGG_OPUS_DECODE_ERROR:
            std::cerr << " (OGG_OPUS_DECODE_ERROR - Decode failed)";
            break;
        default:
            break;
    }
}

int main(int argc, char* argv[]) {
    try {
        if (argc != 3) {
            print_usage(argv[0]);
            return 1;
        }

        const char* input_file = argv[1];
        const char* output_file = argv[2];

        // Open input file
        std::ifstream input(input_file, std::ios::binary);
        if (!input) {
            std::cerr << "Error: Could not open input file: " << input_file << "\n";
            return 1;
        }

        // Create Ogg Opus decoder
        micro_opus::OggOpusDecoder decoder;

        // Input buffer - just read chunks sequentially, decoder handles buffering internally
        const size_t chunk_size = 4096;
        std::vector<uint8_t> input_buffer(chunk_size);

        // Output PCM buffer - start with typical 20ms stereo frame, will auto-resize if needed
        // (20ms at 48kHz stereo = 960 samples * 2 channels = 1920)
        const size_t pcm_buffer_size = static_cast<size_t>(960) * 2;
        std::vector<int16_t> pcm_buffer(pcm_buffer_size);

        WavWriter* wav_writer = nullptr;
        bool decoder_initialized = false;
        size_t total_packets = 0;
        size_t audio_packets = 0;
        size_t total_bytes_read = 0;
        size_t total_bytes_consumed = 0;
        size_t decode_calls = 0;

        // Process file - read chunks and feed directly to decoder
        while (input) {
            // Read a chunk from file
            input.read(reinterpret_cast<char*>(input_buffer.data()), chunk_size);
            std::streamsize bytes_read = input.gcount();

            if (bytes_read == 0) {
                break;  // EOF reached
            }

            total_bytes_read += bytes_read;

            // Decode from this chunk - decoder may need multiple calls per chunk
            size_t chunk_offset = 0;
            while (chunk_offset < static_cast<size_t>(bytes_read)) {
                size_t consumed = 0;
                size_t samples = 0;

                decode_calls++;

                micro_opus::OggOpusResult result = decoder.decode(
                    input_buffer.data() + chunk_offset, bytes_read - chunk_offset,
                    pcm_buffer.data(), pcm_buffer.size() * sizeof(int16_t), consumed, samples);

                total_bytes_consumed += consumed;
                chunk_offset += consumed;

                // Check if decoder is now initialized (after OpusHead parsed)
                if (!decoder_initialized && decoder.is_initialized()) {
                    decoder_initialized = true;

                    uint32_t sample_rate = decoder.get_sample_rate();
                    uint8_t channels = decoder.get_channels();

                    std::cout << "Opus stream info:\n";
                    std::cout << "  Sample rate: " << sample_rate << " Hz\n";
                    std::cout << "  Channels: " << static_cast<int>(channels)
                              << (channels == 1 ? " (mono)" : " (stereo)") << "\n";
                    std::cout << "  Pre-skip: " << decoder.get_pre_skip() << " samples\n";

                    // Create WAV writer with decoder's format
                    wav_writer = new WavWriter(output_file, sample_rate, channels, 16);

                    if (!wav_writer->isOpen()) {
                        std::cerr << "Error: Could not create output file: " << output_file << "\n";
                        delete wav_writer;
                        return 1;
                    }
                }

                // Standard C error checking: result != 0 means error
                if (result != 0) {
                    // Handle buffer too small by resizing and retrying
                    if (result == micro_opus::OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL) {
                        size_t required_bytes = decoder.get_required_output_buffer_size();
                        size_t required_samples = required_bytes / sizeof(int16_t);
                        std::cout << "Resizing PCM buffer from " << pcm_buffer.size() << " to "
                                  << required_samples << " samples\n";
                        pcm_buffer.resize(required_samples);
                        continue;  // Retry decode with larger buffer
                    }

                    // Error occurred - provide detailed error information
                    std::cerr << "Error at byte position " << total_bytes_consumed << " in file\n";
                    std::cerr << "Decode call #" << decode_calls << ", consumed=" << consumed
                              << ", samples=" << samples << "\n";
                    std::cerr << "Error: Decoding failed with error code: "
                              << static_cast<int>(result);
                    print_error_description(result);
                    std::cerr << "\n";
                    { delete wav_writer; }
                    return 1;
                }

                // Success! Check samples to see if we got samples
                if (samples > 0) {
                    total_packets++;
                    audio_packets++;

                    // Write decoded samples to WAV file
                    if (wav_writer) {
                        if (!wav_writer->writeSamples(pcm_buffer.data(), samples)) {
                            std::cerr << "Error: Failed to write samples to WAV file\n";
                            delete wav_writer;
                            return 1;
                        }
                    }
                }

                // If no bytes consumed AND no samples, decoder needs more data - read next chunk
                if (consumed == 0 && samples == 0) {
                    break;  // Exit inner loop, read more from file
                }
            }  // End of inner decode loop
        }  // End of outer file read loop

        // Clean up
        if (wav_writer) {
            std::cout << "\nConversion complete!\n";
            std::cout << "Total decode() calls: " << decode_calls << "\n";
            std::cout << "Total bytes read from file: " << total_bytes_read << "\n";
            std::cout << "Total bytes consumed by decoder: " << total_bytes_consumed << "\n";
            std::cout << "Average bytes per packet: "
                      << (total_bytes_consumed / (total_packets > 0 ? total_packets : 1)) << "\n";
            std::cout << "Total packets decoded: " << total_packets << " (" << audio_packets
                      << " audio packets)\n";
            std::cout << "Total samples written: " << wav_writer->getSamplesWritten() << "\n";
            std::cout << "Duration: "
                      << (wav_writer->getSamplesWritten() /
                          static_cast<double>(decoder.get_sample_rate()))
                      << " seconds\n";
            std::cout << "Output file: " << output_file << "\n";

            delete wav_writer;
        } else {
            std::cerr << "Error: No Opus stream found in input file\n";
            return 1;
        }

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
