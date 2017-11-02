/*
 * Copyright 2017, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PMEM_MEMCPY_SSE2_H
#define PMEM_MEMCPY_SSE2_H

#include <xmmintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "libpmem.h"
#include "out.h"

static inline void
memmove_small_sse2(char *dest, const char *src, size_t len)
{
	ASSERT(len <= 64);

	if (len > 48) {
		/* 49..64 */
		__m128i xmm0 = _mm_loadu_si128((__m128i *)src);
		__m128i xmm1 = _mm_loadu_si128((__m128i *)(src + 16));
		__m128i xmm2 = _mm_loadu_si128((__m128i *)(src + 32));
		__m128i xmm3 = _mm_loadu_si128((__m128i *)(src + len - 16));

		_mm_storeu_si128((__m128i *)dest, xmm0);
		_mm_storeu_si128((__m128i *)(dest + 16), xmm1);
		_mm_storeu_si128((__m128i *)(dest + 32), xmm2);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm3);
	} else if (len > 32) {
		/* 33..48 */
		__m128i xmm0 = _mm_loadu_si128((__m128i *)src);
		__m128i xmm1 = _mm_loadu_si128((__m128i *)(src + 16));
		__m128i xmm2 = _mm_loadu_si128((__m128i *)(src + len - 16));

		_mm_storeu_si128((__m128i *)dest, xmm0);
		_mm_storeu_si128((__m128i *)(dest + 16), xmm1);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm2);
	} else if (len > 16) {
		/* 17..32 */
		__m128i xmm0 = _mm_loadu_si128((__m128i *)src);
		__m128i xmm1 = _mm_loadu_si128((__m128i *)(src + len - 16));

		_mm_storeu_si128((__m128i *)dest, xmm0);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm1);
	} else if (len > 8) {
		/* 9..16 */
		uint64_t d0 = *(uint64_t *)src;
		uint64_t d1 = *(uint64_t *)(src + len - 8);

		*(uint64_t *)dest = d0;
		*(uint64_t *)(dest + len - 8) = d1;
	} else if (len > 4) {
		/* 5..8 */
		uint32_t d0 = *(uint32_t *)src;
		uint32_t d1 = *(uint32_t *)(src + len - 4);

		*(uint32_t *)dest = d0;
		*(uint32_t *)(dest + len - 4) = d1;
	} else if (len > 2) {
		/* 3..4 */
		uint16_t d0 = *(uint16_t *)src;
		uint16_t d1 = *(uint16_t *)(src + len - 2);

		*(uint16_t *)dest = d0;
		*(uint16_t *)(dest + len - 2) = d1;
	} else if (len == 2) {
		*(uint16_t *)dest = *(uint16_t *)src;
	} else {
		*(uint8_t *)dest = *(uint8_t *)src;
	}
}

#endif
