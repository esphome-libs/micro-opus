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

/* Thread-local pseudostack implementation
 *
 * This module provides:
 * 1. The _Thread_local pseudostack pointers (scratch_ptr, global_stack)
 * 2. Automatic cleanup of pseudostack buffers when threads exit via pthread TLS
 */
#include "thread_local_stack.h"

#include <pthread.h>
#include <stddef.h>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
/* Use configured memory preference for pseudostack allocation */
#if defined(CONFIG_OPUS_PSEUDOSTACK_PREFER_PSRAM)
#define OPUS_ALLOC_SCRATCH(size) \
    heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_INTERNAL)
#elif defined(CONFIG_OPUS_PSEUDOSTACK_PREFER_INTERNAL)
#define OPUS_ALLOC_SCRATCH(size) \
    heap_caps_malloc_prefer(size, 2, MALLOC_CAP_INTERNAL, MALLOC_CAP_SPIRAM)
#elif defined(CONFIG_OPUS_PSEUDOSTACK_PSRAM_ONLY)
#define OPUS_ALLOC_SCRATCH(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM)
#elif defined(CONFIG_OPUS_PSEUDOSTACK_INTERNAL_ONLY)
#define OPUS_ALLOC_SCRATCH(size) heap_caps_malloc(size, MALLOC_CAP_INTERNAL)
#else
#define OPUS_ALLOC_SCRATCH(size) \
    heap_caps_malloc_prefer(size, 2, MALLOC_CAP_SPIRAM, MALLOC_CAP_INTERNAL)
#endif
#define OPUS_FREE(ptr) heap_caps_free(ptr)
#define LOG_D(tag, fmt, ...) ESP_LOGD(tag, fmt, ##__VA_ARGS__)
#define LOG_E(tag, fmt, ...) ESP_LOGE(tag, fmt, ##__VA_ARGS__)
#else
#include <stdio.h>
#include <stdlib.h>
#define OPUS_ALLOC_SCRATCH(size) malloc(size)
#define OPUS_FREE(ptr) free(ptr)
#define LOG_D(tag, fmt, ...)
#define LOG_E(tag, fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

/* GLOBAL_STACK_SIZE is defined in arch.h (included via stack_alloc.h) */
#include "arch.h"

static const char* TAG = "opus_tls";

/*
 * Thread-local pseudostack pointers
 * These are the actual storage for scratch_ptr and global_stack, accessed
 * directly from the PUSH/ALLOC/SAVE_STACK/RESTORE_STACK macros for zero overhead.
 */
_Thread_local char* scratch_ptr = NULL;
_Thread_local char* global_stack = NULL;

/* Global pthread TLS key for cleanup tracking */
static pthread_key_t opus_cleanup_key;

/* One-time initialization control */
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

/* Flag to track if initialization succeeded */
static int tls_initialized = 0;

/* Destructor function - automatically called when thread exits */
static void pseudostack_destructor(void* data) {
    char* buffer = (char*)data;
    if (buffer) {
        OPUS_FREE(buffer);
        LOG_D(TAG, "Auto-freed pseudostack buffer for exiting thread");
    }
}

/* One-time initialization function */
static void init_pthread_key(void) {
    int ret = pthread_key_create(&opus_cleanup_key, pseudostack_destructor);
    if (ret == 0) {
        tls_initialized = 1;
        LOG_D(TAG, "pthread TLS key created for pseudostack cleanup");
    } else {
        LOG_E(TAG, "Failed to create pthread TLS key: %d", ret);
    }
}

void register_pseudostack_for_cleanup(char* buffer) {
    /* Ensure one-time initialization has been done */
    pthread_once(&init_once, init_pthread_key);

    if (!tls_initialized) {
        LOG_E(TAG, "pthread TLS initialization failed, cleanup not registered");
        return;
    }

    /* Register the buffer pointer with pthread TLS for automatic cleanup */
    int ret = pthread_setspecific(opus_cleanup_key, buffer);
    if (ret != 0) {
        LOG_E(TAG, "Failed to register pseudostack for cleanup: %d", ret);
    } else {
        LOG_D(TAG, "Registered pseudostack buffer %p for cleanup", (void*)buffer);
    }
}

/* Allocate pseudostack and register for cleanup - called from ALLOC_STACK macro */
char* _opus_alloc_and_register_pseudostack(void) {
    scratch_ptr = (char*)OPUS_ALLOC_SCRATCH(GLOBAL_STACK_SIZE);
    register_pseudostack_for_cleanup(scratch_ptr);
    return scratch_ptr;
}
