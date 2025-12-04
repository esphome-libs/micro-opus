/* Copyright (C) 2013 Xiph.Org Foundation and contributors */
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

#ifndef FIXED_LX7_H
#define FIXED_LX7_H

/** 16x32 multiplication, followed by a 16-bit shift right. Results fits in 32 bits */
#undef MULT16_32_Q16
static OPUS_INLINE opus_int32 MULT16_32_Q16_lx7(opus_int16 a, opus_int32 b) {
    opus_int32 res;
    __asm__ volatile("mulsh %0, %1, %2\n\t" : "=r"(res) : "r"(SHL32(a, 16)), "r"(b));
    return res;
}
#define MULT16_32_Q16(a, b) MULT16_32_Q16_lx7(a, b)

/** 16x32 multiplication, followed by a 16-bit shift right (round-to-nearest). Results fits in 32
 * bits */
#undef MULT16_32_P16
#define MULT16_32_P16(a, b) ((opus_val32)PSHR((opus_int64)((opus_val16)(a)) * (b), 16))

/** 16x32 multiplication, followed by a 15-bit shift right. Results fits in 32 bits */
#undef MULT16_32_Q15
static OPUS_INLINE opus_int32 MULT16_32_Q15_lx7(opus_int16 a, opus_int32 b) {
    opus_int32 res;
    __asm__ volatile("mulsh %0, %1, %2\n\t" : "=r"(res) : "r"(SHL32(a, 16)), "r"(b));
    /* Ignore LSB for speed */
    return SHL32(res, 1);
}
#define MULT16_32_Q15(a, b) MULT16_32_Q15_lx7(a, b)

/** 32x32 multiplication, followed by a 16-bit shift right. Results fits in 32 bits */
#undef MULT32_32_Q16
#define MULT32_32_Q16(a, b) ((opus_val32)SHR((opus_int64)(a) * (opus_int64)(b), 16))

/** 32x32 multiplication, followed by a 31-bit shift right. Results fits in 32 bits */
#undef MULT32_32_Q31
static OPUS_INLINE opus_int32 MULT32_32_Q31_lx7(opus_int32 a, opus_int32 b) {
    opus_int32 res;
    __asm__ volatile("mulsh %0, %1, %2\n\t" : "=r"(res) : "r"(a), "r"(b));
    /* Ignore LSB for speed */
    return SHL32(res, 1);
}
#define MULT32_32_Q31(a, b) MULT32_32_Q31_lx7(a, b)

/** 32x32 multiplication, followed by a 31-bit shift right (with rounding). Results fits in 32 bits
 */
#undef MULT32_32_P31
#define MULT32_32_P31(a, b) ((opus_val32)SHR(1073741824 + (opus_int64)(a) * (opus_int64)(b), 31))

/** 32x32 multiplication, followed by a 32-bit shift right. Results fits in 32 bits */
#undef MULT32_32_Q32
static OPUS_INLINE opus_int32 MULT32_32_Q32_lx7(opus_int32 a, opus_int32 b) {
    opus_int32 res;
    __asm__ volatile("mulsh %0, %1, %2\n\t" : "=r"(res) : "r"(a), "r"(b));
    return res;
}
#define MULT32_32_Q32(a, b) MULT32_32_Q32_lx7(a, b)

/** 16x32 multiply, followed by a 15-bit shift right and 32-bit add.
    b must fit in 31 bits.
    Result fits in 32 bits. */
#undef MAC16_32_Q15
#define MAC16_32_Q15(c, a, b) ADD32(c, MULT16_32_Q15(a, b))

/** 16x32 multiply, followed by a 16-bit shift right and 32-bit add.
    Result fits in 32 bits. */
#undef MAC16_32_Q16
#define MAC16_32_Q16(c, a, b) ADD32(c, MULT16_32_Q16(a, b))

#undef SIG2WORD16
static OPUS_INLINE opus_val16 SIG2WORD16_lx7(celt_sig x) {
    x = PSHR32(x, SIG_SHIFT);
    __asm__("clamps %0, %1, 15\n\t" : "=r"(x) : "r"(x));
    return EXTRACT16(x);
}
#define SIG2WORD16(x) (SIG2WORD16_lx7(x))

#endif
