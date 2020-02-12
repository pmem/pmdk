// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memset_avx.h"

static force_inline void
memset_mov8x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);
	_mm256_store_si256((__m256i *)dest + 2, ymm);
	_mm256_store_si256((__m256i *)dest + 3, ymm);
	_mm256_store_si256((__m256i *)dest + 4, ymm);
	_mm256_store_si256((__m256i *)dest + 5, ymm);
	_mm256_store_si256((__m256i *)dest + 6, ymm);
	_mm256_store_si256((__m256i *)dest + 7, ymm);
	_mm256_store_si256((__m256i *)dest + 8, ymm);
	_mm256_store_si256((__m256i *)dest + 9, ymm);
	_mm256_store_si256((__m256i *)dest + 10, ymm);
	_mm256_store_si256((__m256i *)dest + 11, ymm);
	_mm256_store_si256((__m256i *)dest + 12, ymm);
	_mm256_store_si256((__m256i *)dest + 13, ymm);
	_mm256_store_si256((__m256i *)dest + 14, ymm);
	_mm256_store_si256((__m256i *)dest + 15, ymm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
	flush64b(dest + 4 * 64);
	flush64b(dest + 5 * 64);
	flush64b(dest + 6 * 64);
	flush64b(dest + 7 * 64);
}

static force_inline void
memset_mov4x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);
	_mm256_store_si256((__m256i *)dest + 2, ymm);
	_mm256_store_si256((__m256i *)dest + 3, ymm);
	_mm256_store_si256((__m256i *)dest + 4, ymm);
	_mm256_store_si256((__m256i *)dest + 5, ymm);
	_mm256_store_si256((__m256i *)dest + 6, ymm);
	_mm256_store_si256((__m256i *)dest + 7, ymm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memset_mov2x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);
	_mm256_store_si256((__m256i *)dest + 2, ymm);
	_mm256_store_si256((__m256i *)dest + 3, ymm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memset_mov1x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);

	flush64b(dest + 0 * 64);
}

void
EXPORTED_SYMBOL(char *dest, int c, size_t len)
{
	__m256i ymm = _mm256_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_avx(dest, ymm, cnt);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 8 * 64) {
		memset_mov8x64b(dest, ymm);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_mov4x64b(dest, ymm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_mov2x64b(dest, ymm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_mov1x64b(dest, ymm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memset_small_avx(dest, ymm, len);

	avx_zeroupper();
}
