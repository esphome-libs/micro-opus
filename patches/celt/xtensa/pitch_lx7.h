/***********************************************************************
Copyright (C) 2025 Xiph.Org Foundation and contributors.
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#ifndef PITCH_LX7_H
#define PITCH_LX7_H

#ifdef FIXED_POINT

#define OVERRIDE_DUAL_INNER_PROD

// This method has some overhead and isn't worth using for small N (like most celt_inner_prod
// calls). The speedup is worth the overhead when used for the typical values of N for the
// dual_inner_prod calls.
static OPUS_INLINE opus_val32 dot_prod_lx7(const opus_val16* x, const opus_val16* y, int N) {
    int32_t result;
    const opus_val16* px = x;
    const opus_val16* py = y;
    int loop_cnt = N >> 2;
    int tmp;

    __asm__ volatile(
        // Clear accumulator
        "movi       %[tmp], 0               \n"
        "wsr        %[tmp], acclo           \n"
        "wsr        %[tmp], acchi           \n"

        // Skip to remainder if loop_count is zero
        "beqz       %[cnt], .Lremainder%=   \n"

        // Adjust pointers for ldinc (pre-decrement)
        "addi       %[px], %[px], -4        \n"
        "addi       %[py], %[py], -6        \n"

        // Preload for first iteration
        "ldinc      m0, %[px]               \n"
        "ldinc      m3, %[py]               \n"
        "ldinc      m1, %[px]               \n"

        // Initial multiply for pipeline fill
        "mula.dd.lh.ldinc m2, %[py], m0, m3 \n"
        "ldinc      m3, %[py]               \n"

        // Main loop processing 4 samples per iteration
        "loopnez    %[cnt], .Lloop_end%=    \n"
        "mula.dd.hl.ldinc m0, %[px], m0, m2 \n"
        "mula.dd.lh.ldinc m2, %[py], m1, m2 \n"
        "mula.dd.hl.ldinc m1, %[px], m1, m3 \n"
        "mula.dd.lh.ldinc m3, %[py], m0, m3 \n"
        ".Lloop_end%=:                      \n"

        // Complete final multiplies from pipeline
        "mula.dd.hl m0, m2                  \n"
        "mula.dd.lh m1, m2                  \n"
        "mula.dd.hl m1, m3                  \n"

        ".Lremainder%=:                     \n"
        // Check for 2 more samples (bit 1 of N)
        "bbci       %[n], 1, .Lcheck_one%=  \n"
        "ldinc      m0, %[px]               \n"
        "mula.dd.lh.ldinc m2, %[py], m0, m3 \n"
        "mula.dd.hl m1, m2                  \n"

        ".Lcheck_one%=:                     \n"
        // Check for 1 more sample (bit 0 of N)
        "bbci       %[n], 0, .Ldone%=       \n"
        "ldinc      m0, %[px]               \n"
        "mula.dd.lh m0, m3                  \n"

        ".Ldone%=:                          \n"
        // Get accumulator result
        "rsr        %[res], acclo           \n"

        : [res] "=r"(result), [px] "+r"(px), [py] "+r"(py), [cnt] "+r"(loop_cnt), [tmp] "=&r"(tmp)
        : [n] "r"(N)
        : "memory");

    return result;
}

static OPUS_INLINE void dual_inner_prod(const opus_val16* x, const opus_val16* y01,
                                        const opus_val16* y02, int N, opus_val32* xy1,
                                        opus_val32* xy2, int arch) {
    *xy1 = dot_prod_lx7(x, y01, N);
    *xy2 = dot_prod_lx7(x, y02, N);
}

#endif
#endif
