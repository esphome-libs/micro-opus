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

#ifndef QUANT_BANDS_TIMING_H
#define QUANT_BANDS_TIMING_H

#ifdef CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING

#include <stdio.h>

#ifdef ESP_PLATFORM
#include "esp_timer.h"
#else
#include <time.h>
#endif

/* Quant bands timing statistics structure */
typedef struct {
    int64_t total_time;
    int64_t setup_time;
    int64_t loop_time;
    int64_t quant_band_time;
    int64_t quant_band_stereo_time;
    int64_t opus_copy_time;
    /* quant_band breakdown */
    int64_t deinterleave_hadamard_time;
    int64_t quant_partition_time;
    int64_t interleave_hadamard_time;
    int64_t resynth_time;
    /* quant_partition breakdown */
    int64_t compute_theta_time;
    int64_t alg_unquant_time;
    int64_t fill_operations_time;
    /* alg_unquant breakdown */
    int64_t decode_pulses_time;
    int64_t normalise_residual_time;
    int64_t exp_rotation_time;
    /* exp_rotation breakdown */
    int64_t exp_rotation_setup_time;
    int64_t exp_rotation_cos_time;
    int64_t exp_rotation_rounding_time;
    int64_t exp_rotation_loop_time;
    /* quant_partition counters */
    uint32_t split_path_count;
    uint32_t base_path_count;
    uint32_t max_recursion_depth;
    uint32_t current_recursion_depth;
    int call_count;
} quant_bands_timing_stats_t;

/* Global timing statistics - defined in bands_timing.c */
extern quant_bands_timing_stats_t g_quant_bands_timing;

/* Get current time in microseconds */
static inline int64_t quant_bands_timing_get_time(void) {
#ifdef ESP_PLATFORM
    return esp_timer_get_time();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000LL;
#endif
}

/* Start timing the entire function */
#define QUANT_BANDS_TIMING_START()                           \
    int64_t _qab_func_start = quant_bands_timing_get_time(); \
    int64_t _qab_stage_start = 0

/* End setup phase, start loop phase */
#define QUANT_BANDS_TIMING_START_LOOP()                                    \
    int64_t _qab_setup_end = quant_bands_timing_get_time();                \
    g_quant_bands_timing.setup_time += (_qab_setup_end - _qab_func_start); \
    int64_t _qab_loop_start = _qab_setup_end

/* Time quant_band call - uses stage_start variable */
#define QUANT_BANDS_TIMING_QUANT_BAND_START()             \
    do {                                                  \
        _qab_stage_start = quant_bands_timing_get_time(); \
    } while (0)

#define QUANT_BANDS_TIMING_QUANT_BAND_END()                                          \
    do {                                                                             \
        int64_t _qab_stage_end = quant_bands_timing_get_time();                      \
        g_quant_bands_timing.quant_band_time += (_qab_stage_end - _qab_stage_start); \
    } while (0)

/* Time quant_band_stereo call - uses stage_start variable */
#define QUANT_BANDS_TIMING_QUANT_BAND_STEREO_START()      \
    do {                                                  \
        _qab_stage_start = quant_bands_timing_get_time(); \
    } while (0)

#define QUANT_BANDS_TIMING_QUANT_BAND_STEREO_END()                                          \
    do {                                                                                    \
        int64_t _qab_stage_end = quant_bands_timing_get_time();                             \
        g_quant_bands_timing.quant_band_stereo_time += (_qab_stage_end - _qab_stage_start); \
    } while (0)

/* Time OPUS_COPY calls - uses stage_start variable */
#define QUANT_BANDS_TIMING_OPUS_COPY_START()              \
    do {                                                  \
        _qab_stage_start = quant_bands_timing_get_time(); \
    } while (0)

#define QUANT_BANDS_TIMING_OPUS_COPY_END()                                          \
    do {                                                                            \
        int64_t _qab_stage_end = quant_bands_timing_get_time();                     \
        g_quant_bands_timing.opus_copy_time += (_qab_stage_end - _qab_stage_start); \
    } while (0)

/* Time deinterleave_hadamard - declares its own local variable */
#define QUANT_BANDS_TIMING_DEINTERLEAVE_START() \
    int64_t _qab_deinterleave_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_DEINTERLEAVE_END()                          \
    do {                                                               \
        int64_t _qab_deinterleave_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.deinterleave_hadamard_time +=             \
            (_qab_deinterleave_end - _qab_deinterleave_start);         \
    } while (0)

/* Time quant_partition - declares its own local variable */
#define QUANT_BANDS_TIMING_PARTITION_START() \
    int64_t _qab_partition_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_PARTITION_END()                                                        \
    do {                                                                                          \
        int64_t _qab_partition_end = quant_bands_timing_get_time();                               \
        g_quant_bands_timing.quant_partition_time += (_qab_partition_end - _qab_partition_start); \
    } while (0)

/* Time interleave_hadamard - declares its own local variable */
#define QUANT_BANDS_TIMING_INTERLEAVE_START() \
    int64_t _qab_interleave_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_INTERLEAVE_END()                          \
    do {                                                             \
        int64_t _qab_interleave_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.interleave_hadamard_time +=             \
            (_qab_interleave_end - _qab_interleave_start);           \
    } while (0)

/* Time resynth block - declares its own local variable */
#define QUANT_BANDS_TIMING_RESYNTH_START() \
    int64_t _qab_resynth_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_RESYNTH_END()                                              \
    do {                                                                              \
        int64_t _qab_resynth_end = quant_bands_timing_get_time();                     \
        g_quant_bands_timing.resynth_time += (_qab_resynth_end - _qab_resynth_start); \
    } while (0)

/* Time compute_theta - declares its own local variable */
#define QUANT_BANDS_TIMING_COMPUTE_THETA_START() \
    int64_t _qab_theta_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_COMPUTE_THETA_END()                                          \
    do {                                                                                \
        int64_t _qab_theta_end = quant_bands_timing_get_time();                         \
        g_quant_bands_timing.compute_theta_time += (_qab_theta_end - _qab_theta_start); \
    } while (0)

/* Time alg_unquant - declares its own local variable */
#define QUANT_BANDS_TIMING_ALG_UNQUANT_START() \
    int64_t _qab_unquant_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_ALG_UNQUANT_END()                                              \
    do {                                                                                  \
        int64_t _qab_unquant_end = quant_bands_timing_get_time();                         \
        g_quant_bands_timing.alg_unquant_time += (_qab_unquant_end - _qab_unquant_start); \
    } while (0)

/* Time fill operations - declares its own local variable */
#define QUANT_BANDS_TIMING_FILL_START() int64_t _qab_fill_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_FILL_END()                                                   \
    do {                                                                                \
        int64_t _qab_fill_end = quant_bands_timing_get_time();                          \
        g_quant_bands_timing.fill_operations_time += (_qab_fill_end - _qab_fill_start); \
    } while (0)

/* Recursion tracking */
#define QUANT_BANDS_TIMING_ENTER_RECURSION()                  \
    do {                                                      \
        g_quant_bands_timing.current_recursion_depth++;       \
        if (g_quant_bands_timing.current_recursion_depth >    \
            g_quant_bands_timing.max_recursion_depth) {       \
            g_quant_bands_timing.max_recursion_depth =        \
                g_quant_bands_timing.current_recursion_depth; \
        }                                                     \
    } while (0)

#define QUANT_BANDS_TIMING_EXIT_RECURSION()             \
    do {                                                \
        g_quant_bands_timing.current_recursion_depth--; \
    } while (0)

/* Path counters */
#define QUANT_BANDS_TIMING_COUNT_SPLIT_PATH()    \
    do {                                         \
        g_quant_bands_timing.split_path_count++; \
    } while (0)

#define QUANT_BANDS_TIMING_COUNT_BASE_PATH()    \
    do {                                        \
        g_quant_bands_timing.base_path_count++; \
    } while (0)

/* Time decode_pulses - declares its own local variable */
#define QUANT_BANDS_TIMING_DECODE_PULSES_START() \
    int64_t _qab_decode_pulses_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_DECODE_PULSES_END()                          \
    do {                                                                \
        int64_t _qab_decode_pulses_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.decode_pulses_time +=                      \
            (_qab_decode_pulses_end - _qab_decode_pulses_start);        \
    } while (0)

/* Time normalise_residual - declares its own local variable */
#define QUANT_BANDS_TIMING_NORMALISE_START() \
    int64_t _qab_normalise_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_NORMALISE_END()                          \
    do {                                                            \
        int64_t _qab_normalise_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.normalise_residual_time +=             \
            (_qab_normalise_end - _qab_normalise_start);            \
    } while (0)

/* Time exp_rotation - declares its own local variable */
#define QUANT_BANDS_TIMING_EXP_ROTATION_START() \
    int64_t _qab_exp_rotation_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_EXP_ROTATION_END()                          \
    do {                                                               \
        int64_t _qab_exp_rotation_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.exp_rotation_time +=                      \
            (_qab_exp_rotation_end - _qab_exp_rotation_start);         \
    } while (0)

/* Time exp_rotation setup (gain/theta calculation) */
#define QUANT_BANDS_TIMING_EXP_ROTATION_SETUP_START() \
    int64_t _qab_exp_rot_setup_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_EXP_ROTATION_SETUP_END()                     \
    do {                                                                \
        int64_t _qab_exp_rot_setup_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.exp_rotation_setup_time +=                 \
            (_qab_exp_rot_setup_end - _qab_exp_rot_setup_start);        \
    } while (0)

/* Time exp_rotation cos calculation */
#define QUANT_BANDS_TIMING_EXP_ROTATION_COS_START() \
    int64_t _qab_exp_rot_cos_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_EXP_ROTATION_COS_END()                     \
    do {                                                              \
        int64_t _qab_exp_rot_cos_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.exp_rotation_cos_time +=                 \
            (_qab_exp_rot_cos_end - _qab_exp_rot_cos_start);          \
    } while (0)

/* Time exp_rotation rounding calculation */
#define QUANT_BANDS_TIMING_EXP_ROTATION_ROUNDING_START() \
    int64_t _qab_exp_rot_rounding_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_EXP_ROTATION_ROUNDING_END()                     \
    do {                                                                   \
        int64_t _qab_exp_rot_rounding_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.exp_rotation_rounding_time +=                 \
            (_qab_exp_rot_rounding_end - _qab_exp_rot_rounding_start);     \
    } while (0)

/* Time exp_rotation loop */
#define QUANT_BANDS_TIMING_EXP_ROTATION_LOOP_START() \
    int64_t _qab_exp_rot_loop_start = quant_bands_timing_get_time()

#define QUANT_BANDS_TIMING_EXP_ROTATION_LOOP_END()                     \
    do {                                                               \
        int64_t _qab_exp_rot_loop_end = quant_bands_timing_get_time(); \
        g_quant_bands_timing.exp_rotation_loop_time +=                 \
            (_qab_exp_rot_loop_end - _qab_exp_rot_loop_start);         \
    } while (0)

/* End the entire function */
#define QUANT_BANDS_TIMING_END_TOTAL()                                        \
    do {                                                                      \
        int64_t _qab_func_end = quant_bands_timing_get_time();                \
        g_quant_bands_timing.loop_time += (_qab_func_end - _qab_loop_start);  \
        g_quant_bands_timing.total_time += (_qab_func_end - _qab_func_start); \
        g_quant_bands_timing.call_count++;                                    \
    } while (0)

/* Print statistics every N calls */
#define QUANT_BANDS_TIMING_PRINT(N)                                                                \
    do {                                                                                           \
        if (g_quant_bands_timing.call_count >= (N)) {                                              \
            uint32_t total_partition_calls =                                                       \
                g_quant_bands_timing.split_path_count + g_quant_bands_timing.base_path_count;      \
            printf("\n=== quant_all_bands Timing (averaged over %d frames) ===\n", (N));           \
            printf("Total:                %6lld us\n", g_quant_bands_timing.total_time / (N));     \
            printf("  Setup:              %6lld us  (%.1f%%)\n",                                   \
                   g_quant_bands_timing.setup_time / (N),                                          \
                   100.0 * g_quant_bands_timing.setup_time / g_quant_bands_timing.total_time);     \
            printf("  Loop:               %6lld us  (%.1f%%)\n",                                   \
                   g_quant_bands_timing.loop_time / (N),                                           \
                   100.0 * g_quant_bands_timing.loop_time / g_quant_bands_timing.total_time);      \
            printf(                                                                                \
                "    quant_band:       %6lld us  (%.1f%%)\n",                                      \
                g_quant_bands_timing.quant_band_time / (N),                                        \
                100.0 * g_quant_bands_timing.quant_band_time / g_quant_bands_timing.total_time);   \
            printf("      deinterleave:   %6lld us\n",                                             \
                   g_quant_bands_timing.deinterleave_hadamard_time / (N));                         \
            printf("      quant_partition:%6lld us\n",                                             \
                   g_quant_bands_timing.quant_partition_time / (N));                               \
            if (total_partition_calls > 0) {                                                       \
                printf("        compute_theta:%5lld us  (%u calls, %.1f/frame)\n",                 \
                       g_quant_bands_timing.compute_theta_time / (N),                              \
                       g_quant_bands_timing.split_path_count,                                      \
                       (float)g_quant_bands_timing.split_path_count / (N));                        \
                printf("        alg_unquant:  %5lld us  (%u calls, %.1f/frame)\n",                 \
                       g_quant_bands_timing.alg_unquant_time / (N),                                \
                       g_quant_bands_timing.base_path_count,                                       \
                       (float)g_quant_bands_timing.base_path_count / (N));                         \
                if (g_quant_bands_timing.base_path_count > 0) {                                    \
                    printf("          decode_pulses:  %4lld us  (%.1f%%)\n",                       \
                           g_quant_bands_timing.decode_pulses_time / (N),                          \
                           100.0 * g_quant_bands_timing.decode_pulses_time /                       \
                               g_quant_bands_timing.alg_unquant_time);                             \
                    printf("          normalise:      %4lld us  (%.1f%%)\n",                       \
                           g_quant_bands_timing.normalise_residual_time / (N),                     \
                           100.0 * g_quant_bands_timing.normalise_residual_time /                  \
                               g_quant_bands_timing.alg_unquant_time);                             \
                    printf("          exp_rotation:   %4lld us  (%.1f%%)\n",                       \
                           g_quant_bands_timing.exp_rotation_time / (N),                           \
                           100.0 * g_quant_bands_timing.exp_rotation_time /                        \
                               g_quant_bands_timing.alg_unquant_time);                             \
                    if (g_quant_bands_timing.exp_rotation_time > 0) {                              \
                        printf("            setup:        %4lld us  (%.1f%%)\n",                   \
                               g_quant_bands_timing.exp_rotation_setup_time / (N),                 \
                               100.0 * g_quant_bands_timing.exp_rotation_setup_time /              \
                                   g_quant_bands_timing.exp_rotation_time);                        \
                        printf("            cos:          %4lld us  (%.1f%%)\n",                   \
                               g_quant_bands_timing.exp_rotation_cos_time / (N),                   \
                               100.0 * g_quant_bands_timing.exp_rotation_cos_time /                \
                                   g_quant_bands_timing.exp_rotation_time);                        \
                        printf("            rounding:     %4lld us  (%.1f%%)\n",                   \
                               g_quant_bands_timing.exp_rotation_rounding_time / (N),              \
                               100.0 * g_quant_bands_timing.exp_rotation_rounding_time /           \
                                   g_quant_bands_timing.exp_rotation_time);                        \
                        printf("            loop:         %4lld us  (%.1f%%)\n",                   \
                               g_quant_bands_timing.exp_rotation_loop_time / (N),                  \
                               100.0 * g_quant_bands_timing.exp_rotation_loop_time /               \
                                   g_quant_bands_timing.exp_rotation_time);                        \
                    }                                                                              \
                }                                                                                  \
                printf("        fill_ops:     %5lld us\n",                                         \
                       g_quant_bands_timing.fill_operations_time / (N));                           \
                printf("        recursion: max_depth=%u, split=%u, base=%u\n",                     \
                       g_quant_bands_timing.max_recursion_depth,                                   \
                       g_quant_bands_timing.split_path_count,                                      \
                       g_quant_bands_timing.base_path_count);                                      \
            }                                                                                      \
            printf("      interleave:     %6lld us\n",                                             \
                   g_quant_bands_timing.interleave_hadamard_time / (N));                           \
            printf("      resynth:        %6lld us\n", g_quant_bands_timing.resynth_time / (N));   \
            printf("    quant_b_stereo:   %6lld us  (%.1f%%)\n",                                   \
                   g_quant_bands_timing.quant_band_stereo_time / (N),                              \
                   100.0 * g_quant_bands_timing.quant_band_stereo_time /                           \
                       g_quant_bands_timing.total_time);                                           \
            printf("    OPUS_COPY:        %6lld us  (%.1f%%)\n",                                   \
                   g_quant_bands_timing.opus_copy_time / (N),                                      \
                   100.0 * g_quant_bands_timing.opus_copy_time / g_quant_bands_timing.total_time); \
            printf("==========================================================\n\n");              \
            g_quant_bands_timing.total_time = 0;                                                   \
            g_quant_bands_timing.setup_time = 0;                                                   \
            g_quant_bands_timing.loop_time = 0;                                                    \
            g_quant_bands_timing.quant_band_time = 0;                                              \
            g_quant_bands_timing.quant_band_stereo_time = 0;                                       \
            g_quant_bands_timing.opus_copy_time = 0;                                               \
            g_quant_bands_timing.deinterleave_hadamard_time = 0;                                   \
            g_quant_bands_timing.quant_partition_time = 0;                                         \
            g_quant_bands_timing.interleave_hadamard_time = 0;                                     \
            g_quant_bands_timing.resynth_time = 0;                                                 \
            g_quant_bands_timing.compute_theta_time = 0;                                           \
            g_quant_bands_timing.alg_unquant_time = 0;                                             \
            g_quant_bands_timing.fill_operations_time = 0;                                         \
            g_quant_bands_timing.decode_pulses_time = 0;                                           \
            g_quant_bands_timing.normalise_residual_time = 0;                                      \
            g_quant_bands_timing.exp_rotation_time = 0;                                            \
            g_quant_bands_timing.exp_rotation_setup_time = 0;                                      \
            g_quant_bands_timing.exp_rotation_cos_time = 0;                                        \
            g_quant_bands_timing.exp_rotation_rounding_time = 0;                                   \
            g_quant_bands_timing.exp_rotation_loop_time = 0;                                       \
            g_quant_bands_timing.split_path_count = 0;                                             \
            g_quant_bands_timing.base_path_count = 0;                                              \
            g_quant_bands_timing.max_recursion_depth = 0;                                          \
            g_quant_bands_timing.call_count = 0;                                                   \
        }                                                                                          \
    } while (0)

#else /* CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING */

/* No-op macros when timing is disabled */
#define QUANT_BANDS_TIMING_START() \
    do {                           \
    } while (0)
#define QUANT_BANDS_TIMING_START_LOOP() \
    do {                                \
    } while (0)
#define QUANT_BANDS_TIMING_QUANT_BAND_START() \
    do {                                      \
    } while (0)
#define QUANT_BANDS_TIMING_QUANT_BAND_END() \
    do {                                    \
    } while (0)
#define QUANT_BANDS_TIMING_QUANT_BAND_STEREO_START() \
    do {                                             \
    } while (0)
#define QUANT_BANDS_TIMING_QUANT_BAND_STEREO_END() \
    do {                                           \
    } while (0)
#define QUANT_BANDS_TIMING_OPUS_COPY_START() \
    do {                                     \
    } while (0)
#define QUANT_BANDS_TIMING_OPUS_COPY_END() \
    do {                                   \
    } while (0)
#define QUANT_BANDS_TIMING_DEINTERLEAVE_START() \
    do {                                        \
    } while (0)
#define QUANT_BANDS_TIMING_DEINTERLEAVE_END() \
    do {                                      \
    } while (0)
#define QUANT_BANDS_TIMING_PARTITION_START() \
    do {                                     \
    } while (0)
#define QUANT_BANDS_TIMING_PARTITION_END() \
    do {                                   \
    } while (0)
#define QUANT_BANDS_TIMING_INTERLEAVE_START() \
    do {                                      \
    } while (0)
#define QUANT_BANDS_TIMING_INTERLEAVE_END() \
    do {                                    \
    } while (0)
#define QUANT_BANDS_TIMING_RESYNTH_START() \
    do {                                   \
    } while (0)
#define QUANT_BANDS_TIMING_RESYNTH_END() \
    do {                                 \
    } while (0)
#define QUANT_BANDS_TIMING_COMPUTE_THETA_START() \
    do {                                         \
    } while (0)
#define QUANT_BANDS_TIMING_COMPUTE_THETA_END() \
    do {                                       \
    } while (0)
#define QUANT_BANDS_TIMING_ALG_UNQUANT_START() \
    do {                                       \
    } while (0)
#define QUANT_BANDS_TIMING_ALG_UNQUANT_END() \
    do {                                     \
    } while (0)
#define QUANT_BANDS_TIMING_FILL_START() \
    do {                                \
    } while (0)
#define QUANT_BANDS_TIMING_FILL_END() \
    do {                              \
    } while (0)
#define QUANT_BANDS_TIMING_ENTER_RECURSION() \
    do {                                     \
    } while (0)
#define QUANT_BANDS_TIMING_EXIT_RECURSION() \
    do {                                    \
    } while (0)
#define QUANT_BANDS_TIMING_COUNT_SPLIT_PATH() \
    do {                                      \
    } while (0)
#define QUANT_BANDS_TIMING_COUNT_BASE_PATH() \
    do {                                     \
    } while (0)
#define QUANT_BANDS_TIMING_DECODE_PULSES_START() \
    do {                                         \
    } while (0)
#define QUANT_BANDS_TIMING_DECODE_PULSES_END() \
    do {                                       \
    } while (0)
#define QUANT_BANDS_TIMING_NORMALISE_START() \
    do {                                     \
    } while (0)
#define QUANT_BANDS_TIMING_NORMALISE_END() \
    do {                                   \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_START() \
    do {                                        \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_END() \
    do {                                      \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_SETUP_START() \
    do {                                              \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_SETUP_END() \
    do {                                            \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_COS_START() \
    do {                                            \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_COS_END() \
    do {                                          \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_ROUNDING_START() \
    do {                                                 \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_ROUNDING_END() \
    do {                                               \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_LOOP_START() \
    do {                                             \
    } while (0)
#define QUANT_BANDS_TIMING_EXP_ROTATION_LOOP_END() \
    do {                                           \
    } while (0)
#define QUANT_BANDS_TIMING_END_TOTAL() \
    do {                               \
    } while (0)
#define QUANT_BANDS_TIMING_PRINT(N) \
    do {                            \
    } while (0)

#endif /* CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING */

#endif /* QUANT_BANDS_TIMING_H */
