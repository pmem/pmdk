/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2020, Intel Corporation */

#ifndef PMEM2_MEMSET_SSE2_H
#define PMEM2_MEMSET_SSE2_H

#include <xmmintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "out.h"

static force_inline void
memset_small_sse2_noflush(char *dest, __m128i xmm, size_t len)
{
	ASSERT(len <= 64);

	if (len <= 8)
		goto le8;
	if (len <= 32)
		goto le32;

	if (len > 48) {
		/* 49..64 */
		_mm_storeu_si128((__m128i *)(dest + 0), xmm);
		_mm_storeu_si128((__m128i *)(dest + 16), xmm);
		_mm_storeu_si128((__m128i *)(dest + 32), xmm);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm);
		return;
	}

	/* 33..48 */
	_mm_storeu_si128((__m128i *)(dest + 0), xmm);
	_mm_storeu_si128((__m128i *)(dest + 16), xmm);
	_mm_storeu_si128((__m128i *)(dest + len - 16), xmm);
	return;

le32:
	if (len > 16) {
		/* 17..32 */
		_mm_storeu_si128((__m128i *)(dest + 0), xmm);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm);
		return;
	}

	/* 9..16 */
	uint64_t d8 = (uint64_t)_mm_cvtsi128_si64(xmm);

	*(ua_uint64_t *)dest = d8;
	*(ua_uint64_t *)(dest + len - 8) = d8;
	return;

le8:
	if (len <= 2)
		goto le2;

	if (len > 4) {
		/* 5..8 */
		uint32_t d4 = (uint32_t)_mm_cvtsi128_si32(xmm);

		*(ua_uint32_t *)dest = d4;
		*(ua_uint32_t *)(dest + len - 4) = d4;
		return;
	}

	/* 3..4 */
	uint16_t d2 = (uint16_t)(uint32_t)_mm_cvtsi128_si32(xmm);

	*(ua_uint16_t *)dest = d2;
	*(ua_uint16_t *)(dest + len - 2) = d2;
	return;

le2:
	if (len == 2) {
		uint16_t d2 = (uint16_t)(uint32_t)_mm_cvtsi128_si32(xmm);

		*(ua_uint16_t *)dest = d2;
		return;
	}

	*(uint8_t *)dest = (uint8_t)_mm_cvtsi128_si32(xmm);
}

static force_inline void
memset_small_sse2(char *dest, __m128i xmm, size_t len, flush_fn flush)
{
	/*
	 * pmemcheck complains about "overwritten stores before they were made
	 * persistent" for overlapping stores (last instruction in each code
	 * path) in the optimized version.
	 * libc's memset also does that, so we can't use it here.
	 */
	if (On_pmemcheck) {
		memset_nodrain_generic(dest, (uint8_t)_mm_cvtsi128_si32(xmm),
				len, PMEM2_F_MEM_NOFLUSH, NULL);
	} else {
		memset_small_sse2_noflush(dest, xmm, len);
	}

	flush(dest, len);
}

#endif
