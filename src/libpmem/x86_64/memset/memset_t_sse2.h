/*
 * Copyright 2017-2018, Intel Corporation
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

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memset_sse2.h"

static force_inline void
memset_mov4x64b(char *dest, __m128i xmm)
{
	_mm_store_si128((__m128i *)dest + 0, xmm);
	_mm_store_si128((__m128i *)dest + 1, xmm);
	_mm_store_si128((__m128i *)dest + 2, xmm);
	_mm_store_si128((__m128i *)dest + 3, xmm);
	_mm_store_si128((__m128i *)dest + 4, xmm);
	_mm_store_si128((__m128i *)dest + 5, xmm);
	_mm_store_si128((__m128i *)dest + 6, xmm);
	_mm_store_si128((__m128i *)dest + 7, xmm);
	_mm_store_si128((__m128i *)dest + 8, xmm);
	_mm_store_si128((__m128i *)dest + 9, xmm);
	_mm_store_si128((__m128i *)dest + 10, xmm);
	_mm_store_si128((__m128i *)dest + 11, xmm);
	_mm_store_si128((__m128i *)dest + 12, xmm);
	_mm_store_si128((__m128i *)dest + 13, xmm);
	_mm_store_si128((__m128i *)dest + 14, xmm);
	_mm_store_si128((__m128i *)dest + 15, xmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memset_mov2x64b(char *dest, __m128i xmm)
{
	_mm_store_si128((__m128i *)dest + 0, xmm);
	_mm_store_si128((__m128i *)dest + 1, xmm);
	_mm_store_si128((__m128i *)dest + 2, xmm);
	_mm_store_si128((__m128i *)dest + 3, xmm);
	_mm_store_si128((__m128i *)dest + 4, xmm);
	_mm_store_si128((__m128i *)dest + 5, xmm);
	_mm_store_si128((__m128i *)dest + 6, xmm);
	_mm_store_si128((__m128i *)dest + 7, xmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memset_mov1x64b(char *dest, __m128i xmm)
{
	_mm_store_si128((__m128i *)dest + 0, xmm);
	_mm_store_si128((__m128i *)dest + 1, xmm);
	_mm_store_si128((__m128i *)dest + 2, xmm);
	_mm_store_si128((__m128i *)dest + 3, xmm);

	flush64b(dest + 0 * 64);
}

void
EXPORTED_SYMBOL(char *dest, int c, size_t len)
{
	__m128i xmm = _mm_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_sse2(dest, xmm, cnt);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 4 * 64) {
		memset_mov4x64b(dest, xmm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_mov2x64b(dest, xmm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_mov1x64b(dest, xmm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memset_small_sse2(dest, xmm, len);
}
