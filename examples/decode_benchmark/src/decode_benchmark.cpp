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

/* ESP32-S3 Opus Decode Benchmark
 *
 * Continuously decodes two 30-second Ogg Opus audio clips and reports timing statistics:
 * - MUSIC (CELT codec): High-bitrate stereo orchestral music
 * - SPEECH (SILK codec): Low-bitrate mono spoken word
 *
 * Uses OggOpusDecoder to demux and decode the audio streams.
 *
 * Demonstrates thread safety by testing 1-4 concurrent tasks for each audio type,
 * with tasks pinned to alternating CPU cores.
 *
 * Each task uses its own OggOpusDecoder instance with the thread-safe pseudostack.
 */

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "micro_opus/ogg_opus_decoder.h"
#include "test_audio_music.h"
#include "test_audio_speech.h"

#include <cinttypes>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static const char* const TAG = "DECODE_BENCH";

static const int MAX_CONCURRENT_TASKS = 4;

// Audio test configurations
enum class AudioType : uint8_t {
    MUSIC,  // CELT codec (high-bitrate stereo)
    SPEECH  // SILK codec (low-bitrate mono)
};

struct AudioConfig {
    const char* name;
    const char* codec;
    const uint8_t* data;
    size_t size;
};

static const AudioConfig AUDIO_CONFIGS[] = {
    {"MUSIC", "CELT", test_opus_music_data, test_opus_music_data_size},
    {"SPEECH", "SILK", test_opus_speech_data, test_opus_speech_data_size}};

static const int NUM_AUDIO_TYPES = sizeof(AUDIO_CONFIGS) / sizeof(AUDIO_CONFIGS[0]);

// Statistics structure for tracking timing data
struct Stats {
    int64_t min_us;
    int64_t max_us;
    int64_t sum_us;
    int64_t sum_sq_us;  // For standard deviation calculation
    size_t count;
    size_t total_samples;  // Total audio samples decoded
};

// Results from a decode run
struct DecodeResult {
    Stats frame_stats;
    int64_t total_time_us;
    uint32_t sample_rate;
    int core_id;
    bool success;
};

// Task parameters
struct TaskParams {
    int task_id;
    DecodeResult* result;
    SemaphoreHandle_t done_semaphore;
    int pinned_core;                  // -1 for no pinning, 0 or 1 for specific core
    const AudioConfig* audio_config;  // Which audio to decode
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
    if (time_us < s->min_us) {
        s->min_us = time_us;
    }

    if (time_us > s->max_us) {
        s->max_us = time_us;
    }
    s->sum_us += time_us;
    s->sum_sq_us += time_us * time_us;
    s->count++;
    s->total_samples += samples;
}

// Log statistics
static void log_stats(const char* prefix, const char* name, Stats* s) {
    if (s->count == 0) {
        ESP_LOGI(TAG, "%s%s: no data", prefix, name);
        return;
    }

    double avg = (double)s->sum_us / s->count;
    double variance = (double)s->sum_sq_us / s->count - avg * avg;
    double stddev = sqrt(variance);

    ESP_LOGI(TAG, "%s%s (us): min=%" PRId64 " max=%" PRId64 " avg=%.1f sd=%.1f (n=%zu)", prefix,
             name, s->min_us, s->max_us, avg, stddev, s->count);
}

// Decode the full test audio file and return results
static DecodeResult decode_full_file(const uint8_t* audio_data, size_t audio_size) {
    DecodeResult result{};
    init_stats(&result.frame_stats);
    result.success = true;
    result.sample_rate = 0;
    result.core_id = xPortGetCoreID();

    // Create decoder
    micro_opus::OggOpusDecoder decoder;

    // PCM output buffer - allocated once headers are parsed and we know the format
    std::vector<int16_t> pcm_buffer;

    // Input data pointers
    const uint8_t* input_ptr = audio_data;
    size_t input_remaining = audio_size;

    // Start timing
    int64_t iteration_start = esp_timer_get_time();

    // Decode loop
    while (input_remaining > 0) {
        size_t bytes_consumed = 0;
        size_t samples_decoded = 0;

        // Time this decode call
        int64_t frame_start = esp_timer_get_time();

        micro_opus::OggOpusResult decode_result =
            decoder.decode(input_ptr, input_remaining, pcm_buffer.data(),
                           pcm_buffer.size() * sizeof(int16_t), bytes_consumed, samples_decoded);

        int64_t frame_time = esp_timer_get_time() - frame_start;

        // Once initialized, allocate PCM buffer for typical 20ms frame (will auto-resize if needed)
        if (pcm_buffer.empty() && decoder.is_initialized()) {
            size_t samples_per_20ms = decoder.get_sample_rate() / 50;
            size_t buffer_size = samples_per_20ms * decoder.get_channels();
            pcm_buffer.resize(buffer_size);
        }

        // Update statistics only when samples were decoded
        if (samples_decoded > 0) {
            update_stats(&result.frame_stats, frame_time, samples_decoded);
        }

        // Check for errors
        if (decode_result != micro_opus::OGG_OPUS_OK) {
            // Handle buffer too small by resizing and retrying
            if (decode_result == micro_opus::OGG_OPUS_OUTPUT_BUFFER_TOO_SMALL) {
                size_t required_bytes = decoder.get_required_output_buffer_size();
                size_t required_samples = required_bytes / sizeof(int16_t);
                ESP_LOGI(TAG, "Resizing PCM buffer from %zu to %zu samples", pcm_buffer.size(),
                         required_samples);
                pcm_buffer.resize(required_samples);
                continue;  // Retry decode with larger buffer
            }
            result.success = false;
            break;
        }

        // Advance input pointer
        input_ptr += bytes_consumed;
        input_remaining -= bytes_consumed;

        // Prevent infinite loops
        if (bytes_consumed == 0 && input_remaining > 0) {
            result.success = false;
            break;
        }

        // Yield to allow other tasks to run (important for concurrent decoding)
        taskYIELD();
    }

    result.total_time_us = esp_timer_get_time() - iteration_start;
    result.sample_rate = decoder.get_sample_rate();

    return result;
}

// Log decode results with optional prefix
static void log_decode_result(const char* prefix, DecodeResult* result) {
    if (!result->success) {
        ESP_LOGE(TAG, "%sDecode failed", prefix);
        return;
    }

    log_stats(prefix, "Frame", &result->frame_stats);

    // Calculate real-time factor
    double audio_duration_us =
        (double)result->frame_stats.total_samples / result->sample_rate * 1000000.0;
    double rtf = (double)result->total_time_us / audio_duration_us;

    ESP_LOGI(TAG, "%sTotal: %" PRId64 " ms (%.1fs audio), RTF: %.3f (%.1fx real-time), core %d",
             prefix, result->total_time_us / 1000, audio_duration_us / 1000000.0, rtf, 1.0 / rtf,
             result->core_id);
}

// FreeRTOS task function for concurrent decoding
static void decode_task(void* params) {
    TaskParams* task_params = (TaskParams*)params;
    const AudioConfig* config = task_params->audio_config;

    ESP_LOGI(TAG, "Task %d starting %s decode...", task_params->task_id, config->name);

    // Decode the full file
    *task_params->result = decode_full_file(config->data, config->size);

    ESP_LOGI(TAG, "Task %d finished (%" PRId64 " ms)", task_params->task_id,
             task_params->result->total_time_us / 1000);

    // Signal completion
    xSemaphoreGive(task_params->done_semaphore);

    // Delete this task
    vTaskDelete(NULL);
}

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-S3 Opus Decode Benchmark ===");
    ESP_LOGI(TAG, "Audio types: %d (MUSIC/CELT, SPEECH/SILK)", NUM_AUDIO_TYPES);
    for (int a = 0; a < NUM_AUDIO_TYPES; a++) {
        ESP_LOGI(TAG, "  %s (%s): %zu bytes", AUDIO_CONFIGS[a].name, AUDIO_CONFIGS[a].codec,
                 AUDIO_CONFIGS[a].size);
    }
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    ESP_LOGI(TAG, "Free Internal: %lu bytes", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Thread safety test: up to %d concurrent tasks", MAX_CONCURRENT_TASKS);

    // Create semaphore for task synchronization
    SemaphoreHandle_t done_semaphore = xSemaphoreCreateCounting(MAX_CONCURRENT_TASKS, 0);
    if (done_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return;
    }

    uint32_t iteration = 0;

    while (true) {
        iteration++;
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "=== Iteration %lu ===", iteration);

        // Track times for each audio type and task count
        int64_t times[NUM_AUDIO_TYPES][MAX_CONCURRENT_TASKS] = {{0}};
        bool all_success = true;

        // Interleave audio types: for each task count, test both audio types
        for (int num_tasks = 1; num_tasks <= MAX_CONCURRENT_TASKS; num_tasks++) {
            for (int audio_idx = 0; audio_idx < NUM_AUDIO_TYPES; audio_idx++) {
                const AudioConfig* config = &AUDIO_CONFIGS[audio_idx];

                ESP_LOGI(TAG, "");
                ESP_LOGI(TAG, "--- %s (%s) - %d concurrent task%s ---", config->name, config->codec,
                         num_tasks, num_tasks == 1 ? "" : "s");

                DecodeResult results[MAX_CONCURRENT_TASKS];
                TaskParams params[MAX_CONCURRENT_TASKS];

                // Set up task parameters
                for (int i = 0; i < num_tasks; i++) {
                    params[i].task_id = i;
                    params[i].result = &results[i];
                    params[i].done_semaphore = done_semaphore;
                    params[i].pinned_core = i % 2;
                    params[i].audio_config = config;
                }

                int64_t start_time = esp_timer_get_time();

                // Create all tasks pinned to alternating cores
                int tasks_created = 0;
                for (int i = 0; i < num_tasks; i++) {
                    char task_name[16];
                    snprintf(task_name, sizeof(task_name), "decode_%d", i);

                    BaseType_t ret =
                        xTaskCreatePinnedToCore(decode_task, task_name, 8192, &params[i],
                                                1,  // Priority
                                                NULL,
                                                i % 2  // Core ID: alternates 0, 1, 0, 1
                        );

                    if (ret == pdPASS) {
                        tasks_created++;
                    } else {
                        ESP_LOGE(TAG, "Failed to create task %d", i);
                    }
                }

                // Wait for all successfully created tasks to complete
                for (int i = 0; i < tasks_created; i++) {
                    xSemaphoreTake(done_semaphore, portMAX_DELAY);
                }

                times[audio_idx][num_tasks - 1] = esp_timer_get_time() - start_time;

                // Log per-task results
                for (int i = 0; i < num_tasks; i++) {
                    char prefix[16];
                    snprintf(prefix, sizeof(prefix), "Task %d: ", i);
                    log_decode_result(prefix, &results[i]);
                    all_success = all_success && results[i].success;
                }
            }
        }

        // --- Summary ---
        ESP_LOGI(TAG, "");
        ESP_LOGI(TAG, "--- Summary ---");
        for (int audio_idx = 0; audio_idx < NUM_AUDIO_TYPES; audio_idx++) {
            const AudioConfig* config = &AUDIO_CONFIGS[audio_idx];
            ESP_LOGI(TAG, "%s (%s):", config->name, config->codec);
            for (int i = 0; i < MAX_CONCURRENT_TASKS; i++) {
                ESP_LOGI(TAG, "  %d task%s  %6" PRId64 " ms", i + 1,
                         i == 0 ? ": " : "s:", times[audio_idx][i] / 1000);
            }
        }
        ESP_LOGI(TAG, "All decodes successful: %s", all_success ? "YES" : "NO");
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
        ESP_LOGI(TAG, "---");

        // Small delay between iterations
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
