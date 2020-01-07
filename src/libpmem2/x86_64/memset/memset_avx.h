// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2019, Intel Corporation */

#ifndef PMEM2_MEMSET_AVX_H
#define PMEM2_MEMSET_AVX_H

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "avx.h"
#include "out.h"

static force_inline void
memset_small_avx_noflush(char *dest, __m256i ymm, size_t len)
{
	ASSERT(len <= 64);

	if (len <= 8)
		goto le8;
	if (len <= 32)
		goto le32;

	/* 33..64 */
	_mm256_storeu_si256((__m256i *)dest, ymm);
	_mm256_storeu_si256((__m256i *)(dest + len - 32), ymm);
	return;

le32:
	if (len > 16) {
		/* 17..32 */
		__m128i xmm = m256_get16b(ymm);

		_mm_storeu_si128((__m128i *)dest, xmm);
		_mm_storeu_si128((__m128i *)(dest + len - 16), xmm);
		return;
	}

	/* 9..16 */
	uint64_t d8 = m256_get8b(ymm);

	*(ua_uint64_t *)dest = d8;
	*(ua_uint64_t *)(dest + len - 8) = d8;
	return;

le8:
	if (len <= 2)
		goto le2;

	if (len > 4) {
		/* 5..8 */
		uint32_t d = m256_get4b(ymm);

		*(ua_uint32_t *)dest = d;
		*(ua_uint32_t *)(dest + len - 4) = d;
		return;
	}

	/* 3..4 */
	uint16_t d2 = m256_get2b(ymm);

	*(ua_uint16_t *)dest = d2;
	*(ua_uint16_t *)(dest + len - 2) = d2;
	return;

le2:
	if (len == 2) {
		uint16_t d2 = m256_get2b(ymm);

		*(ua_uint16_t *)dest = d2;
		return;
	}

	*(uint8_t *)dest = (uint8_t)m256_get2b(ymm);
}

static force_inline void
memset_small_avx(char *dest, __m256i ymm, size_t len)
{
	memset_small_avx_noflush(dest, ymm, len);
	flush(dest, len);
}

#endif
