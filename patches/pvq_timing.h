/* Copyright (c) 2025 Kevin Ahrendt */
/*
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
   OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef PVQ_TIMING_H
#define PVQ_TIMING_H

#ifdef CONFIG_OPUS_ENABLE_PVQ_TIMING

#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <time.h>
#endif

/* PVQ timing statistics structure */
typedef struct {
    int64_t decode_pulses_time;
    int64_t normalise_residual_time;
    int64_t exp_rotation_time;
    int64_t exp_rotation1_time;
    int64_t extract_collapse_mask_time;
    int64_t total_pvq_time;
    int call_count;
} pvq_timing_stats_t;

/* Global PVQ timing statistics */
static pvq_timing_stats_t g_pvq_timing = {0};

/* Get current time in microseconds */
static inline int64_t pvq_timing_get_time(void) {
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
#endif
}

/* Start timing a section - declares both variables */
#define PVQ_TIMING_START()                             \
    int64_t _pvq_timing_start = pvq_timing_get_time(); \
    int64_t _pvq_timing_stage_start = 0

/* Start timing a stage within PVQ - re-assigns the variable */
#define PVQ_TIMING_START_STAGE()                         \
    do {                                                 \
        _pvq_timing_stage_start = pvq_timing_get_time(); \
    } while (0)

/* Start timing exp_rotation1 calls */
#define PVQ_TIMING_START_ROTATION1() int64_t _pvq_timing_rotation1_start = pvq_timing_get_time()

/* End timing and accumulate to a specific counter */
#define PVQ_TIMING_END(counter)                                              \
    do {                                                                     \
        int64_t _pvq_timing_end = pvq_timing_get_time();                     \
        g_pvq_timing.counter += (_pvq_timing_end - _pvq_timing_stage_start); \
    } while (0)

/* End timing for rotation1 specifically */
#define PVQ_TIMING_END_ROTATION1(counter)                                        \
    do {                                                                         \
        int64_t _pvq_timing_end = pvq_timing_get_time();                         \
        g_pvq_timing.counter += (_pvq_timing_end - _pvq_timing_rotation1_start); \
    } while (0)

/* End total PVQ timing */
#define PVQ_TIMING_END_TOTAL(counter)                                  \
    do {                                                               \
        int64_t _pvq_timing_end = pvq_timing_get_time();               \
        g_pvq_timing.counter += (_pvq_timing_end - _pvq_timing_start); \
    } while (0)

/* Print timing statistics every N calls */
#define PVQ_TIMING_PRINT(N)                                                                    \
    do {                                                                                       \
        g_pvq_timing.call_count++;                                                             \
        if (g_pvq_timing.call_count >= (N)) {                                                  \
            printf("\n=== PVQ Decoding Timing (averaged over %d calls) ===\n", (N));           \
            printf("Pulse Decoding:       %6lld us\n", g_pvq_timing.decode_pulses_time / (N)); \
            printf("Residual Normalize:   %6lld us\n",                                         \
                   g_pvq_timing.normalise_residual_time / (N));                                \
            printf("Rotation (total):     %6lld us\n", g_pvq_timing.exp_rotation_time / (N));  \
            printf("  - exp_rotation1:    %6lld us\n", g_pvq_timing.exp_rotation1_time / (N)); \
            printf("Collapse Mask:        %6lld us\n",                                         \
                   g_pvq_timing.extract_collapse_mask_time / (N));                             \
            printf("----------------------------------------\n");                              \
            printf("TOTAL PVQ:            %6lld us\n", g_pvq_timing.total_pvq_time / (N));     \
            printf("========================================\n\n");                            \
            g_pvq_timing.decode_pulses_time = 0;                                               \
            g_pvq_timing.normalise_residual_time = 0;                                          \
            g_pvq_timing.exp_rotation_time = 0;                                                \
            g_pvq_timing.exp_rotation1_time = 0;                                               \
            g_pvq_timing.extract_collapse_mask_time = 0;                                       \
            g_pvq_timing.total_pvq_time = 0;                                                   \
            g_pvq_timing.call_count = 0;                                                       \
        }                                                                                      \
    } while (0)

#else /* CONFIG_OPUS_ENABLE_PVQ_TIMING */

/* No-op macros when timing is disabled */
#define PVQ_TIMING_START() \
    do {                   \
    } while (0)
#define PVQ_TIMING_START_STAGE() \
    do {                         \
    } while (0)
#define PVQ_TIMING_START_ROTATION1() \
    do {                             \
    } while (0)
#define PVQ_TIMING_END(counter) \
    do {                        \
    } while (0)
#define PVQ_TIMING_END_ROTATION1(counter) \
    do {                                  \
    } while (0)
#define PVQ_TIMING_END_TOTAL(counter) \
    do {                              \
    } while (0)
#define PVQ_TIMING_PRINT(N) \
    do {                    \
    } while (0)

#endif /* CONFIG_OPUS_ENABLE_PVQ_TIMING */

#endif /* PVQ_TIMING_H */
