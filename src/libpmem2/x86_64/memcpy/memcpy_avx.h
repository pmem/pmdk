/*
 * Copyright 2017-2019, Intel Corporation
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

#ifndef PMEM2_MEMCPY_AVX_H
#define PMEM2_MEMCPY_AVX_H

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "out.h"

static force_inline void
memmove_small_avx_noflush(char *dest, const char *src, size_t len)
{
	ASSERT(len <= 64);

	if (len <= 8)
		goto le8;
	if (len <= 32)
		goto le32;

	/* 33..64 */
	__m256i ymm0 = _mm256_loadu_si256((__m256i *)src);
	__m256i ymm1 = _mm256_loadu_si256((__m256i *)(src + len - 32));

	_mm256_storeu_si256((__m256i *)dest, ymm0);
	_mm256_storeu_si256((__m256i *)(dest + len - 32), ymm1);
	return;

le32:
	if (len > 16) {
		/* 17..32 */
		__m128i xmm0 = _mm_loadu_si128((__m128i *)src);
		__m128i xmm1 = _mm_loadu_si128((__m128i *)(src + len - 16));

		_mm_storeu_si128((__m128i *)dest, xmm0);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm1);
		return;
	}

	/* 9..16 */
	ua_uint64_t d80 = *(ua_uint64_t *)src;
	ua_uint64_t d81 = *(ua_uint64_t *)(src + len - 8);

	*(ua_uint64_t *)dest = d80;
	*(ua_uint64_t *)(dest + len - 8) = d81;
	return;

le8:
	if (len <= 2)
		goto le2;

	if (len > 4) {
		/* 5..8 */
		ua_uint32_t d40 = *(ua_uint32_t *)src;
		ua_uint32_t d41 = *(ua_uint32_t *)(src + len - 4);

		*(ua_uint32_t *)dest = d40;
		*(ua_uint32_t *)(dest + len - 4) = d41;
		return;
	}

	/* 3..4 */
	ua_uint16_t d20 = *(ua_uint16_t *)src;
	ua_uint16_t d21 = *(ua_uint16_t *)(src + len - 2);

	*(ua_uint16_t *)dest = d20;
	*(ua_uint16_t *)(dest + len - 2) = d21;
	return;

le2:
	if (len == 2) {
		*(ua_uint16_t *)dest = *(ua_uint16_t *)src;
		return;
	}

	*(uint8_t *)dest = *(uint8_t *)src;
}

static force_inline void
memmove_small_avx(char *dest, const char *src, size_t len)
{
	memmove_small_avx_noflush(dest, src, len);
	flush(dest, len);
}

#endif
