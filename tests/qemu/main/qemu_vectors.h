// Copyright 2026 Kevin Ahrendt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Table of RFC 8251 test-vector ".bit" inputs embedded in the firmware. The
// definition lives in vectors_data.c, generated from tests/vectors/*.bit by
// embed_vectors.py at configure time.

#ifndef MICRO_OPUS_QEMU_VECTORS_H_
#define MICRO_OPUS_QEMU_VECTORS_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    const char* name;           // e.g. "testvector01"
    const unsigned char* data;  // opus_demo ".bit" framing
    unsigned int len;           // byte length of data
} qemu_vector_t;

extern const qemu_vector_t QEMU_VECTORS[];
extern const unsigned int QEMU_VECTOR_COUNT;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MICRO_OPUS_QEMU_VECTORS_H_
