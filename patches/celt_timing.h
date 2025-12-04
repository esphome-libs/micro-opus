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

#ifndef CELT_TIMING_H
#define CELT_TIMING_H

#ifdef CONFIG_OPUS_ENABLE_CELT_TIMING

#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <time.h>
#endif

/* Timing statistics structure */
typedef struct {
    int64_t entropy_decode_time;
    int64_t pvq_decode_time;
    int64_t energy_finalize_time;
    int64_t synthesis_time;
    int64_t postfilter_time;
    int64_t deemphasis_time;
    int64_t total_time;
    int call_count;
} celt_timing_stats_t;

/* Global timing statistics */
static celt_timing_stats_t g_celt_timing = {0};

/* Get current time in microseconds */
static inline int64_t celt_timing_get_time(void) {
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
#endif
}

/* Start timing a section */
#define CELT_TIMING_START() int64_t _timing_start = celt_timing_get_time()

/* End timing and accumulate to a specific counter */
#define CELT_TIMING_END(counter)                                \
    do {                                                        \
        int64_t _timing_end = celt_timing_get_time();           \
        g_celt_timing.counter += (_timing_end - _timing_start); \
    } while (0)

/* Print timing statistics every N calls */
#define CELT_TIMING_PRINT(N)                                                                   \
    do {                                                                                       \
        g_celt_timing.call_count++;                                                            \
        if (g_celt_timing.call_count >= (N)) {                                                 \
            printf("\n=== CELT Decoder Timing (averaged over %d frames) ===\n", (N));          \
            printf("Entropy Decoding:  %6lld us\n", g_celt_timing.entropy_decode_time / (N));  \
            printf("PVQ Decoding:      %6lld us\n", g_celt_timing.pvq_decode_time / (N));      \
            printf("Energy Finalize:   %6lld us\n", g_celt_timing.energy_finalize_time / (N)); \
            printf("Synthesis (IMDCT): %6lld us\n", g_celt_timing.synthesis_time / (N));       \
            printf("Post-filtering:    %6lld us\n", g_celt_timing.postfilter_time / (N));      \
            printf("Deemphasis:        %6lld us\n", g_celt_timing.deemphasis_time / (N));      \
            printf("----------------------------------------\n");                              \
            printf("TOTAL:             %6lld us\n", g_celt_timing.total_time / (N));           \
            printf("========================================\n\n");                            \
            g_celt_timing.entropy_decode_time = 0;                                             \
            g_celt_timing.pvq_decode_time = 0;                                                 \
            g_celt_timing.energy_finalize_time = 0;                                            \
            g_celt_timing.synthesis_time = 0;                                                  \
            g_celt_timing.postfilter_time = 0;                                                 \
            g_celt_timing.deemphasis_time = 0;                                                 \
            g_celt_timing.total_time = 0;                                                      \
            g_celt_timing.call_count = 0;                                                      \
        }                                                                                      \
    } while (0)

#else /* CONFIG_OPUS_ENABLE_CELT_TIMING */

/* No-op macros when timing is disabled */
#define CELT_TIMING_START() \
    do {                    \
    } while (0)
#define CELT_TIMING_END(counter) \
    do {                         \
    } while (0)
#define CELT_TIMING_PRINT(N) \
    do {                     \
    } while (0)

#endif /* CONFIG_OPUS_ENABLE_CELT_TIMING */

#endif /* CELT_TIMING_H */
