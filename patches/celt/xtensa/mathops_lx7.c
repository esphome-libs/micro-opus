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

#include "mathops_lx7.h"

#if !defined(DISABLE_FLOAT_API) && defined(OPUS_XTENSA_LX7)

void celt_float2int16_lx7(const float* OPUS_RESTRICT in, short* OPUS_RESTRICT out, int cnt) {
    int32_t tmp;
    __asm__ volatile("loopnez %[cnt], 1f\n\t"
                     "lsi f0, %[in], 0\n\t"
                     "addi %[in], %[in], 4\n\t"
                     "round.s %[tmp], f0, 15\n\t"
                     "clamps %[tmp], %[tmp], 15\n\t"
                     "s16i %[tmp], %[out], 0\n\t"
                     "addi %[out], %[out], 2\n\t"
                     "1:\n\t"
                     : [in] "+r"(in), [out] "+r"(out), [tmp] "=&r"(tmp)
                     : [cnt] "r"(cnt)
                     : "f0", "memory");
}

#endif
