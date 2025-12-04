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

/* Thread-local pseudostack for THREADSAFE_PSEUDOSTACK mode
 *
 * This module provides:
 * 1. _Thread_local pseudostack pointers (scratch_ptr, global_stack) for zero-overhead access
 * 2. Automatic cleanup via pthread TLS destructor when threads exit
 *
 * The pseudostack pointers are accessed directly by the PUSH/ALLOC macros in stack_alloc.h.
 * Cleanup registration uses pthread TLS solely for the destructor callback.
 */
#ifndef THREAD_LOCAL_STACK_H
#define THREAD_LOCAL_STACK_H

#include <stddef.h>

/* Register a pseudostack buffer for automatic cleanup when the thread exits.
 * Uses pthread TLS with a destructor callback to free the buffer.
 * Called once per thread during lazy allocation in ALLOC_STACK.
 */
void register_pseudostack_for_cleanup(char* buffer);

/* Allocate pseudostack and register for cleanup.
 * Called from ALLOC_STACK macro on first use in each thread.
 * Returns the allocated scratch_ptr.
 */
char* _opus_alloc_and_register_pseudostack(void);

#endif /* THREAD_LOCAL_STACK_H */
