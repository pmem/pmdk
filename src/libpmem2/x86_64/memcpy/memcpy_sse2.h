/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

#ifndef PMEM2_MEMCPY_SSE2_H
#define PMEM2_MEMCPY_SSE2_H

#include <xmmintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "out.h"

static force_inline void
memmove_small_sse2_noflush(char *dest, const char *src, size_t len)
{
	ASSERT(len <= 64);

	if (len <= 8)
		goto le8;
	if (len <= 32)
		goto le32;

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
		return;
	}

	/* 33..48 */
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)(src + 16));
	__m128i xmm2 = _mm_loadu_si128((__m128i *)(src + len - 16));

	_mm_storeu_si128((__m128i *)dest, xmm0);
	_mm_storeu_si128((__m128i *)(dest + 16), xmm1);
	_mm_storeu_si128((__m128i *)(dest + len - 16), xmm2);
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
	uint64_t d80 = *(ua_uint64_t *)src;
	uint64_t d81 = *(ua_uint64_t *)(src + len - 8);

	*(ua_uint64_t *)dest = d80;
	*(ua_uint64_t *)(dest + len - 8) = d81;
	return;

le8:
	if (len <= 2)
		goto le2;

	if (len > 4) {
		/* 5..8 */
		uint32_t d40 = *(ua_uint32_t *)src;
		uint32_t d41 = *(ua_uint32_t *)(src + len - 4);

		*(ua_uint32_t *)dest = d40;
		*(ua_uint32_t *)(dest + len - 4) = d41;
		return;
	}

	/* 3..4 */
	uint16_t d20 = *(ua_uint16_t *)src;
	uint16_t d21 = *(ua_uint16_t *)(src + len - 2);

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
memmove_small_sse2(char *dest, const char *src, size_t len, flush_fn flush)
{
	/*
	 * pmemcheck complains about "overwritten stores before they were made
	 * persistent" for overlapping stores (last instruction in each code
	 * path) in the optimized version.
	 * libc's memcpy also does that, so we can't use it here.
	 */
	if (On_pmemcheck) {
		memmove_nodrain_generic(dest, src, len, PMEM2_F_MEM_NOFLUSH,
				NULL);
	} else {
		memmove_small_sse2_noflush(dest, src, len);
	}

	flush(dest, len);
}

#endif
