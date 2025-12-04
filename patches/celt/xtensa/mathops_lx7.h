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

#ifndef MATHOPS_LX7_H
#define MATHOPS_LX7_H

#include "opus_defines.h"

#ifndef DISABLE_FLOAT_API

#define PI 3.141592653f

#define OVERRIDE_CELT_SQRT (1)
#define celt_sqrt(x) (sqrtf(x))

#define OVERRIDE_CELT_COS_NORM (1)
#define celt_cos_norm(x) (cosf((.5f * PI) * (x)))

void celt_float2int16_lx7(const float* OPUS_RESTRICT in, short* OPUS_RESTRICT out, int cnt);

#define OVERRIDE_FLOAT2INT16 (1)
#define celt_float2int16(in, out, cnt, arch) ((void)(arch), celt_float2int16_lx7(in, out, cnt))

#endif
#endif
