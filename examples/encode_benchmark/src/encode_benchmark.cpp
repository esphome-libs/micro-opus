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

/* ESP32-S3 Opus Encode Benchmark
 *
 * Benchmarks Opus encoding at various settings using two 30-second audio clips:
 * - SPEECH (mono): Tests low-bitrate encoding, typically using SILK codec
 * - MUSIC (stereo): Tests high-bitrate encoding, typically using CELT codec
 *
 * For each encoder configuration, the benchmark:
 * 1. Decodes the Opus file packet by packet using OggOpusDecoder
 * 2. Immediately encodes each decoded PCM frame using the raw Opus encoder API
 * 3. Times ONLY the encoding step (not decoding)
 * 4. Reports statistics: min/max/avg/stddev frame times, RTF, actual bitrate
 *
 * Tests a full matrix of: complexity levels x application modes x bitrates
 */

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "micro_opus/ogg_opus_decoder.h"
#include "opus.h"
#include "test_audio_music.h"
#include "test_audio_speech.h"
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static const char* TAG = "ENCODE_BENCH";

// Audio test configurations
struct AudioConfig {
    const char* name;
    const char* preferred_codec;  // SILK or CELT (what we expect encoder to use)
    const uint8_t* data;
    size_t size;
    uint8_t channels;
    uint32_t sample_rate;  // Output sample rate for decode/encode
};

static const AudioConfig AUDIO_CONFIGS[] = {
    {"SPEECH", "SILK", test_opus_speech_data, test_opus_speech_data_size, 1, 16000},  // 16kHz mono
    {"MUSIC", "CELT", test_opus_music_data, test_opus_music_data_size, 2, 48000}};  // 48kHz stereo

static const int NUM_AUDIO_TYPES = sizeof(AUDIO_CONFIGS) / sizeof(AUDIO_CONFIGS[0]);

// Encoder test configuration
struct EncoderConfig {
    int complexity;      // 0-10
    int application;     // OPUS_APPLICATION_VOIP or OPUS_APPLICATION_AUDIO
    int target_bitrate;  // bps
    const char* mode_name;
};

// Speech-optimized configurations (lower bitrates)
static const EncoderConfig SPEECH_CONFIGS[] = {
    // VOIP mode (prefers SILK)
    {0, OPUS_APPLICATION_VOIP, 10000, "VOIP"},
    {0, OPUS_APPLICATION_VOIP, 16000, "VOIP"},
    {0, OPUS_APPLICATION_VOIP, 24000, "VOIP"},
    {0, OPUS_APPLICATION_VOIP, 32000, "VOIP"},
    {2, OPUS_APPLICATION_VOIP, 10000, "VOIP"},
    {2, OPUS_APPLICATION_VOIP, 16000, "VOIP"},
    {2, OPUS_APPLICATION_VOIP, 24000, "VOIP"},
    {2, OPUS_APPLICATION_VOIP, 32000, "VOIP"},
    {5, OPUS_APPLICATION_VOIP, 10000, "VOIP"},
    {5, OPUS_APPLICATION_VOIP, 16000, "VOIP"},
    {5, OPUS_APPLICATION_VOIP, 24000, "VOIP"},
    {5, OPUS_APPLICATION_VOIP, 32000, "VOIP"},
    {8, OPUS_APPLICATION_VOIP, 10000, "VOIP"},
    {8, OPUS_APPLICATION_VOIP, 16000, "VOIP"},
    {8, OPUS_APPLICATION_VOIP, 24000, "VOIP"},
    {8, OPUS_APPLICATION_VOIP, 32000, "VOIP"},
    {10, OPUS_APPLICATION_VOIP, 10000, "VOIP"},
    {10, OPUS_APPLICATION_VOIP, 16000, "VOIP"},
    {10, OPUS_APPLICATION_VOIP, 24000, "VOIP"},
    {10, OPUS_APPLICATION_VOIP, 32000, "VOIP"},
    // AUDIO mode (prefers CELT, but may use SILK at low bitrates)
    {0, OPUS_APPLICATION_AUDIO, 10000, "AUDIO"},
    {0, OPUS_APPLICATION_AUDIO, 16000, "AUDIO"},
    {0, OPUS_APPLICATION_AUDIO, 24000, "AUDIO"},
    {0, OPUS_APPLICATION_AUDIO, 32000, "AUDIO"},
    {2, OPUS_APPLICATION_AUDIO, 10000, "AUDIO"},
    {2, OPUS_APPLICATION_AUDIO, 16000, "AUDIO"},
    {2, OPUS_APPLICATION_AUDIO, 24000, "AUDIO"},
    {2, OPUS_APPLICATION_AUDIO, 32000, "AUDIO"},
    {5, OPUS_APPLICATION_AUDIO, 10000, "AUDIO"},
    {5, OPUS_APPLICATION_AUDIO, 16000, "AUDIO"},
    {5, OPUS_APPLICATION_AUDIO, 24000, "AUDIO"},
    {5, OPUS_APPLICATION_AUDIO, 32000, "AUDIO"},
    {8, OPUS_APPLICATION_AUDIO, 10000, "AUDIO"},
    {8, OPUS_APPLICATION_AUDIO, 16000, "AUDIO"},
    {8, OPUS_APPLICATION_AUDIO, 24000, "AUDIO"},
    {8, OPUS_APPLICATION_AUDIO, 32000, "AUDIO"},
    {10, OPUS_APPLICATION_AUDIO, 10000, "AUDIO"},
    {10, OPUS_APPLICATION_AUDIO, 16000, "AUDIO"},
    {10, OPUS_APPLICATION_AUDIO, 24000, "AUDIO"},
    {10, OPUS_APPLICATION_AUDIO, 32000, "AUDIO"},
};
static const int NUM_SPEECH_CONFIGS = sizeof(SPEECH_CONFIGS) / sizeof(SPEECH_CONFIGS[0]);

// Music-optimized configurations (higher bitrates, AUDIO mode only)
static const EncoderConfig MUSIC_CONFIGS[] = {
    {0, OPUS_APPLICATION_AUDIO, 64000, "AUDIO"},   {0, OPUS_APPLICATION_AUDIO, 96000, "AUDIO"},
    {0, OPUS_APPLICATION_AUDIO, 128000, "AUDIO"},  {0, OPUS_APPLICATION_AUDIO, 192000, "AUDIO"},
    {2, OPUS_APPLICATION_AUDIO, 64000, "AUDIO"},   {2, OPUS_APPLICATION_AUDIO, 96000, "AUDIO"},
    {2, OPUS_APPLICATION_AUDIO, 128000, "AUDIO"},  {2, OPUS_APPLICATION_AUDIO, 192000, "AUDIO"},
    {5, OPUS_APPLICATION_AUDIO, 64000, "AUDIO"},   {5, OPUS_APPLICATION_AUDIO, 96000, "AUDIO"},
    {5, OPUS_APPLICATION_AUDIO, 128000, "AUDIO"},  {5, OPUS_APPLICATION_AUDIO, 192000, "AUDIO"},
    {8, OPUS_APPLICATION_AUDIO, 64000, "AUDIO"},   {8, OPUS_APPLICATION_AUDIO, 96000, "AUDIO"},
    {8, OPUS_APPLICATION_AUDIO, 128000, "AUDIO"},  {8, OPUS_APPLICATION_AUDIO, 192000, "AUDIO"},
    {10, OPUS_APPLICATION_AUDIO, 64000, "AUDIO"},  {10, OPUS_APPLICATION_AUDIO, 96000, "AUDIO"},
    {10, OPUS_APPLICATION_AUDIO, 128000, "AUDIO"}, {10, OPUS_APPLICATION_AUDIO, 192000, "AUDIO"},
};
static const int NUM_MUSIC_CONFIGS = sizeof(MUSIC_CONFIGS) / sizeof(MUSIC_CONFIGS[0]);

// Statistics structure for tracking timing data
struct Stats {
    int64_t min_us;
    int64_t max_us;
    int64_t sum_us;
    int64_t sum_sq_us;  // For standard deviation calculation
    size_t count;
    size_t total_samples;  // Total audio samples encoded
};

// Results from an encode run
struct EncodeResult {
    Stats frame_stats;
    int64_t total_encode_time_us;  // Total time spent encoding (not decoding)
    size_t total_bytes_encoded;
    double actual_bitrate;
    double rtf;
    uint32_t sample_rate;  // For audio duration calculation
    bool success;
};

// Initialize statistics structure
static void init_stats(Stats* s) {
    s->min_us = INT64_MAX;
    s->max_us = 0;
    s->sum_us = 0;
    s->sum_sq_us = 0;
    s->count = 0;
    s->total_samples = 0;
}

// Update statistics with new timing value and sample count
static void update_stats(Stats* s, int64_t time_us, size_t samples) {
    if (time_us < s->min_us)
        s->min_us = time_us;
    if (time_us > s->max_us)
        s->max_us = time_us;
    s->sum_us += time_us;
    s->sum_sq_us += time_us * time_us;
    s->count++;
    s->total_samples += samples;
}

// Log statistics
static void log_stats(const char* prefix, const char* name, const Stats* s) {
    if (s->count == 0) {
        ESP_LOGI(TAG, "%s%s: no data", prefix, name);
        return;
    }

    double avg = (double)s->sum_us / s->count;
    double variance = (double)s->sum_sq_us / s->count - avg * avg;
    double stddev = sqrt(variance > 0 ? variance : 0);

    ESP_LOGI(TAG, "%s%s (us): min=%" PRId64 " max=%" PRId64 " avg=%.1f sd=%.1f (n=%zu)", prefix,
             name, s->min_us, s->max_us, avg, stddev, s->count);
}

// Run a single encoder configuration test
// Decodes the audio packet by packet, encoding each frame and timing only the encode step
static EncodeResult run_encode_test(const AudioConfig* audio, const EncoderConfig* config) {
    EncodeResult result;
    init_stats(&result.frame_stats);
    result.success = true;
    result.total_bytes_encoded = 0;
    result.total_encode_time_us = 0;

    // Create decoder for input with configured sample rate and channels
    micro_opus::OggOpusDecoder decoder(false, audio->sample_rate, audio->channels);

    // Create encoder at same sample rate
    int error;
    OpusEncoder* encoder =
        opus_encoder_create(audio->sample_rate, audio->channels, config->application, &error);

    if (error != OPUS_OK || encoder == nullptr) {
        ESP_LOGE(TAG, "Failed to create encoder: %s", opus_strerror(error));
        result.success = false;
        return result;
    }

    // Configure encoder
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(config->complexity));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(config->target_bitrate));

    // Frame size for encoding: 20ms at sample rate
    const int FRAME_SIZE = audio->sample_rate / 50;  // 320 for 16kHz, 960 for 48kHz

    // Buffers - static to avoid stack overflow
    // Size for max case: 960 samples * 2 channels * 2 frames = 3840 samples
    static int16_t decode_buffer[960 * 2 * 2];
    static int16_t accum_buffer[960 * 2 * 2];
    static uint8_t encoded_output[16000];  // Max recommended Opus packet size
    size_t accum_samples = 0;              // Samples per channel in accum_buffer

    // Input tracking
    const uint8_t* input_ptr = audio->data;
    size_t input_remaining = audio->size;

    // Process packet by packet
    while (input_remaining > 0 || accum_samples >= FRAME_SIZE) {
        // If we have enough samples, encode a frame
        while (accum_samples >= FRAME_SIZE) {
            int64_t encode_start = esp_timer_get_time();

            opus_int32 encoded_bytes = opus_encode(encoder, accum_buffer, FRAME_SIZE,
                                                   encoded_output, sizeof(encoded_output));

            int64_t encode_time = esp_timer_get_time() - encode_start;

            if (encoded_bytes < 0) {
                ESP_LOGE(TAG, "Encode error: %s", opus_strerror(encoded_bytes));
                result.success = false;
                break;
            }

            result.total_bytes_encoded += encoded_bytes;
            result.total_encode_time_us += encode_time;
            update_stats(&result.frame_stats, encode_time, FRAME_SIZE);

            // Shift remaining samples to front of buffer
            size_t samples_to_shift = accum_samples - FRAME_SIZE;
            if (samples_to_shift > 0) {
                memmove(accum_buffer, accum_buffer + FRAME_SIZE * audio->channels,
                        samples_to_shift * audio->channels * sizeof(int16_t));
            }
            accum_samples -= FRAME_SIZE;
        }

        if (!result.success)
            break;

        // Decode more samples if available
        if (input_remaining > 0) {
            size_t bytes_consumed = 0;
            size_t samples_decoded = 0;

            micro_opus::OggOpusResult decode_result = decoder.decode(
                input_ptr, input_remaining, decode_buffer,
                sizeof(decode_buffer) / sizeof(decode_buffer[0]), bytes_consumed, samples_decoded);

            input_ptr += bytes_consumed;
            input_remaining -= bytes_consumed;

            // Append decoded samples to accumulation buffer
            if (samples_decoded > 0) {
                memcpy(accum_buffer + accum_samples * audio->channels, decode_buffer,
                       samples_decoded * audio->channels * sizeof(int16_t));
                accum_samples += samples_decoded;
            }

            if (decode_result != micro_opus::OGG_OPUS_OK) {
                break;  // End of stream or error
            }

            if (bytes_consumed == 0 && input_remaining > 0) {
                ESP_LOGE(TAG, "Decode stalled");
                result.success = false;
                break;
            }
        }

        taskYIELD();
    }

    // Store sample rate and calculate actual bitrate and RTF
    result.sample_rate = audio->sample_rate;
    if (result.frame_stats.total_samples > 0) {
        double audio_duration_s = (double)result.frame_stats.total_samples / audio->sample_rate;
        result.actual_bitrate = (double)result.total_bytes_encoded * 8.0 / audio_duration_s;
        result.rtf = (double)result.total_encode_time_us / (audio_duration_s * 1000000.0);
    } else {
        result.actual_bitrate = 0;
        result.rtf = 0;
    }

    opus_encoder_destroy(encoder);

    return result;
}

// Log encode results
static void log_encode_result(const EncodeResult* result, const EncoderConfig* config) {
    if (!result->success) {
        ESP_LOGE(TAG, "Encode test failed");
        return;
    }

    log_stats("", "Frame", &result->frame_stats);

    double audio_duration_s = (double)result->frame_stats.total_samples / result->sample_rate;

    ESP_LOGI(TAG, "Total: %" PRId64 " ms (%.1fs audio), RTF: %.3f (%.1fx real-time)",
             result->total_encode_time_us / 1000, audio_duration_s, result->rtf,
             result->rtf > 0 ? 1.0 / result->rtf : 0);

    ESP_LOGI(TAG, "Encoded: %zu bytes (%.0f bps actual, target %d bps)",
             result->total_bytes_encoded, result->actual_bitrate, config->target_bitrate);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 Opus Encode Benchmark ===");
    ESP_LOGI(TAG, "Audio sources:");
    for (int a = 0; a < NUM_AUDIO_TYPES; a++) {
        ESP_LOGI(TAG, "  %s (%s): %zu bytes, %lu kHz, %d channel%s", AUDIO_CONFIGS[a].name,
                 AUDIO_CONFIGS[a].preferred_codec, AUDIO_CONFIGS[a].size,
                 AUDIO_CONFIGS[a].sample_rate / 1000, AUDIO_CONFIGS[a].channels,
                 AUDIO_CONFIGS[a].channels > 1 ? "s" : "");
    }
    ESP_LOGI(TAG, "Processing: decode packet -> encode packet (timing encode only)");
    ESP_LOGI(TAG, "Test matrix: %d speech configs + %d music configs = %d total",
             NUM_SPEECH_CONFIGS, NUM_MUSIC_CONFIGS, NUM_SPEECH_CONFIGS + NUM_MUSIC_CONFIGS);
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free Internal: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

    uint32_t iteration = 0;

    while (true) {
        iteration++;
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "========== Iteration %lu ==========", iteration);

        bool all_success = true;

        // Run SPEECH tests
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=== SPEECH Encoding Tests (%d configurations) ===", NUM_SPEECH_CONFIGS);
        const AudioConfig* speech = &AUDIO_CONFIGS[0];
        int speech_tests_run = 0;

        for (int i = 0; i < NUM_SPEECH_CONFIGS; i++) {
            const EncoderConfig* config = &SPEECH_CONFIGS[i];

            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "--- SPEECH: %s, complexity=%d, bitrate=%d ---", config->mode_name,
                     config->complexity, config->target_bitrate);

            EncodeResult result = run_encode_test(speech, config);
            log_encode_result(&result, config);
            all_success = all_success && result.success;
            speech_tests_run++;

            // Skip remaining tests if encoding is slower than real-time
            if (result.rtf > 1.0) {
                ESP_LOGW(
                    TAG,
                    "RTF > 1.0, skipping remaining SPEECH tests (higher settings will be slower)");
                break;
            }
        }

        // Run MUSIC tests
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=== MUSIC Encoding Tests (%d configurations) ===", NUM_MUSIC_CONFIGS);
        const AudioConfig* music = &AUDIO_CONFIGS[1];
        int music_tests_run = 0;

        for (int i = 0; i < NUM_MUSIC_CONFIGS; i++) {
            const EncoderConfig* config = &MUSIC_CONFIGS[i];

            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "--- MUSIC: %s, complexity=%d, bitrate=%d ---", config->mode_name,
                     config->complexity, config->target_bitrate);

            EncodeResult result = run_encode_test(music, config);
            log_encode_result(&result, config);
            all_success = all_success && result.success;
            music_tests_run++;

            // Skip remaining tests if encoding is slower than real-time
            if (result.rtf > 1.0) {
                ESP_LOGW(
                    TAG,
                    "RTF > 1.0, skipping remaining MUSIC tests (higher settings will be slower)");
                break;
            }
        }

        // Summary
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=== Iteration %lu Summary ===", iteration);
        ESP_LOGI(TAG, "All encodes successful: %s", all_success ? "YES" : "NO");
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "");

        // Small delay between iterations
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
