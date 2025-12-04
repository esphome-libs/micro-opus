/* Copyright (C) 2025 Xiph.Org Foundation contributors */
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

/* Custom support for Opus component with custom allocation */
#ifndef CUSTOM_SUPPORT_H
#define CUSTOM_SUPPORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
/* ESP-IDF build: Use PSRAM-aware allocation */
#include "esp_heap_caps.h"

/* Override opus_alloc to use configurable memory allocation for Opus state/tables */
#define OVERRIDE_OPUS_ALLOC

static inline void* opus_alloc(size_t size) {
#if defined(CONFIG_OPUS_STATE_PREFER_PSRAM)
    /* Try PSRAM first, fall back to internal RAM */
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_STATE_PREFER_INTERNAL)
    /* Try internal RAM first, fall back to PSRAM - must be 8-bit accessible (not IRAM) */
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_STATE_PSRAM_ONLY)
    /* PSRAM only - fail if unavailable */
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_STATE_INTERNAL_ONLY)
    /* Internal RAM only - must be 8-bit accessible (not IRAM) */
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    /* Default: prefer PSRAM, fall back to internal */
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
}

/* Override opus_free to use heap_caps_free */
#define OVERRIDE_OPUS_FREE

static inline void opus_free(void* ptr) {
    heap_caps_free(ptr);
}

/* Override opus_alloc_scratch to allocate the pseudostack with configurable memory preference.
 * This function is called by stack_alloc.h to allocate the pseudostack buffer.
 * For THREADSAFE_PSEUDOSTACK mode, each thread gets its own buffer.
 * For NONTHREADSAFE_PSEUDOSTACK mode, there is one global buffer. */
#define OVERRIDE_OPUS_ALLOC_SCRATCH

static inline void* opus_alloc_scratch(size_t size) {
#if defined(CONFIG_OPUS_PSEUDOSTACK_PREFER_PSRAM)
    /* Try PSRAM first, fall back to internal RAM - must be 8-bit accessible (not IRAM) */
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_PSEUDOSTACK_PREFER_INTERNAL)
    /* Try internal RAM first, fall back to PSRAM - must be 8-bit accessible (not IRAM) */
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_PSEUDOSTACK_PSRAM_ONLY)
    /* PSRAM only - fail if unavailable - must be 8-bit accessible */
    return heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#elif defined(CONFIG_OPUS_PSEUDOSTACK_INTERNAL_ONLY)
    /* Internal RAM only - must be 8-bit accessible (not IRAM) */
    return heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#else
    /* Default: prefer PSRAM, fall back to internal - must be 8-bit accessible (not IRAM) */
    return heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
#endif
}

#endif /* ESP_PLATFORM */

/* Host builds use the default Opus implementations (malloc/free) from os_support.h */

/* Function called on pseudostack overflow - required for pseudostack modes.
 * Called by the PUSH() macro when allocation exceeds GLOBAL_STACK_SIZE. */
static inline void celt_fatal(const char* str, const char* file, int line) {
    printf("FATAL ERROR: %s at %s:%d\n", str, file, line);
    abort();
}

#endif /* CUSTOM_SUPPORT_H */
