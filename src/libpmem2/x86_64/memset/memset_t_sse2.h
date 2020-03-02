// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
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
	LOG(15, "dest %p c %d len %zu", dest, c, len);

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
