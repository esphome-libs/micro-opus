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

#ifndef SILK_SIGPROC_FLP_LX7_H
#define SILK_SIGPROC_FLP_LX7_H

/* Override float2int() macro with Xtensa ROUND.S instruction */
#undef float2int
static OPUS_INLINE opus_int32 float2int_lx7(float x) {
    opus_int32 result;
    __asm__("round.s %0, %1, 0\n\t" : "=r"(result) : "f"(x));
    return result;
}
#define float2int(x) (float2int_lx7(x))

/* Optimized float-to-int16 array conversion with saturation */
#undef silk_float2short_array
static OPUS_INLINE void silk_float2short_array_lx7(opus_int16* out, const silk_float* in,
                                                   opus_int32 length) {
    opus_int32 k;
    for (k = length - 1; k >= 0; k--) {
        opus_int32 result;
        __asm__("round.s %0, %1, 0\n\t"
                "clamps %0, %0, 15\n\t"
                : "=r"(result)
                : "f"(in[k]));
        out[k] = (opus_int16)result;
    }
}
#define silk_float2short_array(out, in, length) (silk_float2short_array_lx7(out, in, length))

/* Optimized int16-to-float array conversion */
#undef silk_short2float_array
static OPUS_INLINE void silk_short2float_array_lx7(silk_float* out, const opus_int16* in,
                                                   opus_int32 length) {
    opus_int32 k;
    for (k = length - 1; k >= 0; k--) {
        silk_float result;
        __asm__("float.s %0, %1, 0\n\t" : "=f"(result) : "r"((opus_int32)in[k]));
        out[k] = result;
    }
}
#define silk_short2float_array(out, in, length) (silk_short2float_array_lx7(out, in, length))

#endif /* SILK_SIGPROC_FLP_LX7_H */
