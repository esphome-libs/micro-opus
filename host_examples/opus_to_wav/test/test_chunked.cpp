/* Test chunked decoding with very small chunks */

#include "micro_opus/ogg_opus_decoder.h"

#include <cstring>
#include <exception>
#include <fstream>
#include <iostream>
#include <vector>

// Test constants
constexpr size_t OPUS_FRAME_SIZE = 960;          // 20ms @ 48kHz
constexpr size_t PROGRESS_INTERVAL = 1000;       // Print progress every N iterations
constexpr size_t MAX_ITERATIONS = 100000;        // Safety limit for infinite loop detection
constexpr size_t VERBOSE_DECODE_THRESHOLD = 20;  // Print details for first N decode calls

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <input.opus>\n";
            return 1;
        }

        const char* input_file = argv[1];

        std::ifstream input(input_file, std::ios::binary);
        if (!input) {
            std::cerr << "Error: Could not open input file: " << input_file << "\n";
            return 1;
        }

        micro_opus::OggOpusDecoder decoder;

        // Test with VERY small chunks (64 bytes) to stress-test buffering
        const size_t tiny_chunk = 64;
        std::vector<uint8_t> input_buffer(tiny_chunk * 4);
        size_t buffer_used = 0;

        // Output PCM buffer
        const size_t pcm_buffer_size = static_cast<size_t>(OPUS_FRAME_SIZE * 2);
        std::vector<int16_t> pcm_buffer(pcm_buffer_size);

        size_t total_samples = 0;
        size_t total_packets = 0;
        size_t decode_calls = 0;
        size_t need_more_data_count = 0;

        bool eof_reached = false;
        size_t iterations = 0;
        while (input || buffer_used > 0) {
            iterations++;
            if (iterations % PROGRESS_INTERVAL == 0) {
                std::cout << "Iteration " << iterations << ", buffer_used=" << buffer_used
                          << ", eof=" << eof_reached << "\n";
            }
            if (iterations > MAX_ITERATIONS) {
                std::cerr << "ERROR: Infinite loop detected!\n";
                return 1;
            }

            // Read tiny chunks
            if (input && buffer_used < input_buffer.size() && !eof_reached) {
                size_t space_available = input_buffer.size() - buffer_used;
                size_t to_read = (space_available < tiny_chunk) ? space_available : tiny_chunk;

                input.read(reinterpret_cast<char*>(input_buffer.data() + buffer_used), to_read);
                std::streamsize bytes_read = input.gcount();

                if (bytes_read == 0) {
                    eof_reached = true;
                    if (buffer_used == 0) {
                        break;
                    }
                } else {
                    buffer_used += bytes_read;
                }
            }

            // Decode
            while (buffer_used > 0) {
                size_t consumed = 0;
                size_t samples = 0;

                decode_calls++;
                micro_opus::OggOpusResult result =
                    decoder.decode(input_buffer.data(), buffer_used, pcm_buffer.data(),
                                   pcm_buffer.size() * sizeof(int16_t), consumed, samples);

                if (decode_calls < VERBOSE_DECODE_THRESHOLD ||
                    decode_calls % PROGRESS_INTERVAL == 0) {
                    std::cout << "decode() call " << decode_calls
                              << ": result=" << static_cast<int>(result)
                              << ", consumed=" << consumed << ", samples=" << samples << "\n";
                }

                if (consumed > 0) {
                    if (consumed < buffer_used) {
                        memmove(input_buffer.data(), input_buffer.data() + consumed,
                                buffer_used - consumed);
                    }
                    buffer_used -= consumed;
                }

                // Standard C error checking: result != 0 means error
                if (result != 0) {
                    std::cerr << "Decode error: " << static_cast<int>(result) << "\n";
                    return 1;
                }

                // Success! Check if we got samples
                if (samples > 0) {
                    total_samples += samples;
                    total_packets++;
                } else {
                    // samples == 0 means we need more data
                    need_more_data_count++;
                    break;
                }
            }

            // Break if EOF and buffer empty
            if (eof_reached && buffer_used == 0) {
                break;
            }
        }

        std::cout << "\nTest results (64-byte chunks):\n";
        std::cout << "  Total decode() calls: " << decode_calls << "\n";
        std::cout << "  Times needed more data: " << need_more_data_count << "\n";
        std::cout << "  Total packets decoded: " << total_packets << "\n";
        std::cout << "  Total samples: " << total_samples << "\n";
        std::cout << "  Duration: " << (total_samples / 48000.0) << " seconds\n";
        std::cout << "\nTest PASSED - decoder handled tiny chunks correctly!\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
