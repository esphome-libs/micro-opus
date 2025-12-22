/* Measure zero-copy effectiveness for Ogg Opus files
 *
 * NOTE: This tool requires MICRO_OGG_DEMUXER_DEBUG to be defined when building.
 */

#ifndef MICRO_OGG_DEMUXER_DEBUG
#error "This tool requires MICRO_OGG_DEMUXER_DEBUG to be defined"
#endif

#include "micro_opus/ogg_opus_decoder.h"

#include <exception>
#include <fstream>
#include <iostream>
#include <vector>

// Buffer size constants
constexpr size_t OPUS_FRAME_SIZE = 960;      // 20ms @ 48kHz
constexpr size_t MAX_FRAMES_PER_PACKET = 6;  // Max frames in an Opus packet
constexpr size_t STEREO_CHANNELS = 2;        // Stereo output
constexpr size_t PCM_BUFFER_SAMPLES = OPUS_FRAME_SIZE * STEREO_CHANNELS * MAX_FRAMES_PER_PACKET;

int main(int argc, char* argv[]) {
    try {
        if (argc != 2) {
            std::cerr << "Usage: " << argv[0] << " <input.opus>\n";
            return 1;
        }

        std::ifstream input(argv[1], std::ios::binary);
        if (!input) {
            std::cerr << "Cannot open file\n";
            return 1;
        }

        // Use OggOpusDecoder to get statistics
        micro_opus::OggOpusDecoder decoder;
        std::vector<uint8_t> input_buffer(4096);              // 4KB chunks like opus_to_wav
        std::vector<int16_t> pcm_buffer(PCM_BUFFER_SAMPLES);  // Large enough for any packet

        size_t total_audio_packets = 0;
        size_t offset = 0;

        while (input) {
            input.read(reinterpret_cast<char*>(input_buffer.data() + offset),
                       input_buffer.size() - offset);
            size_t bytes_read = input.gcount();
            offset += bytes_read;

            while (offset > 0) {
                size_t consumed = 0;
                size_t samples = 0;

                micro_opus::OggOpusResult result =
                    decoder.decode(input_buffer.data(), offset, pcm_buffer.data(),
                                   pcm_buffer.size() * sizeof(int16_t), consumed, samples);

                if (result < 0 && consumed == 0) {
                    // Real error (not just need-more-data)
                    std::cerr << "Decode error: " << static_cast<int>(result) << "\n";
                    return 1;
                }

                if (samples > 0) {
                    total_audio_packets++;
                }

                if (consumed == 0) {
                    // Need more data
                    break;
                }

                // Move remaining data to front
                if (consumed > 0) {
                    if (consumed < offset) {
                        memmove(input_buffer.data(), input_buffer.data() + consumed,
                                offset - consumed);
                    }
                    offset -= consumed;
                }
            }

            if (bytes_read == 0) {
                break;
            }
        }

        // Get statistics
        size_t zero_copy = 0, buffered = 0;
        decoder.get_demuxer_stats(zero_copy, buffered);

        size_t current_buffer_size = 0, max_buffer_size = 0;
        decoder.get_buffer_stats(current_buffer_size, max_buffer_size);

        size_t total_packets = zero_copy + buffered;

        std::cout << "Zero-Copy Statistics\n";
        std::cout << "====================\n\n";
        std::cout << "Total audio packets:  " << total_audio_packets << "\n";
        std::cout << "Total demuxed packets: " << total_packets << " (includes headers)\n\n";
        std::cout << "Zero-copy packets:    " << zero_copy << " ("
                  << (total_packets > 0 ? (100.0 * zero_copy / total_packets) : 0) << "%)\n";
        std::cout << "Buffered packets:     " << buffered << " ("
                  << (total_packets > 0 ? (100.0 * buffered / total_packets) : 0) << "%)\n";

        std::cout << "\nBuffer Statistics:\n";
        std::cout << "  Current capacity:   " << current_buffer_size << " bytes\n";
        std::cout << "  Maximum reached:    " << max_buffer_size << " bytes\n";

        std::cout << "\nBreakdown:\n";
        std::cout
            << "  - Zero-copy: Packet returned directly from user's input buffer (no memcpy)\n";
        std::cout << "  - Buffered:  Packet required copying to internal buffer\n";
        std::cout << "               (incomplete in input, or spans multiple pages)\n";

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
