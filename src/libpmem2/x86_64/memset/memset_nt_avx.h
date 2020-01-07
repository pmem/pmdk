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
#include "out.h"
#include "valgrind_internal.h"

static force_inline void
memset_movnt8x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);
	_mm256_stream_si256((__m256i *)dest + 2, ymm);
	_mm256_stream_si256((__m256i *)dest + 3, ymm);
	_mm256_stream_si256((__m256i *)dest + 4, ymm);
	_mm256_stream_si256((__m256i *)dest + 5, ymm);
	_mm256_stream_si256((__m256i *)dest + 6, ymm);
	_mm256_stream_si256((__m256i *)dest + 7, ymm);
	_mm256_stream_si256((__m256i *)dest + 8, ymm);
	_mm256_stream_si256((__m256i *)dest + 9, ymm);
	_mm256_stream_si256((__m256i *)dest + 10, ymm);
	_mm256_stream_si256((__m256i *)dest + 11, ymm);
	_mm256_stream_si256((__m256i *)dest + 12, ymm);
	_mm256_stream_si256((__m256i *)dest + 13, ymm);
	_mm256_stream_si256((__m256i *)dest + 14, ymm);
	_mm256_stream_si256((__m256i *)dest + 15, ymm);

	VALGRIND_DO_FLUSH(dest, 8 * 64);
}

static force_inline void
memset_movnt4x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);
	_mm256_stream_si256((__m256i *)dest + 2, ymm);
	_mm256_stream_si256((__m256i *)dest + 3, ymm);
	_mm256_stream_si256((__m256i *)dest + 4, ymm);
	_mm256_stream_si256((__m256i *)dest + 5, ymm);
	_mm256_stream_si256((__m256i *)dest + 6, ymm);
	_mm256_stream_si256((__m256i *)dest + 7, ymm);

	VALGRIND_DO_FLUSH(dest, 4 * 64);
}

static force_inline void
memset_movnt2x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);
	_mm256_stream_si256((__m256i *)dest + 2, ymm);
	_mm256_stream_si256((__m256i *)dest + 3, ymm);

	VALGRIND_DO_FLUSH(dest, 2 * 64);
}

static force_inline void
memset_movnt1x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);

	VALGRIND_DO_FLUSH(dest, 64);
}

static force_inline void
memset_movnt1x32b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest, ymm);

	VALGRIND_DO_FLUSH(dest, 32);
}

static force_inline void
memset_movnt1x16b(char *dest, __m256i ymm)
{
	__m128i xmm0 = m256_get16b(ymm);

	_mm_stream_si128((__m128i *)dest, xmm0);

	VALGRIND_DO_FLUSH(dest - 16, 16);
}

static force_inline void
memset_movnt1x8b(char *dest, __m256i ymm)
{
	uint64_t x = m256_get8b(ymm);

	_mm_stream_si64((long long *)dest, (long long)x);

	VALGRIND_DO_FLUSH(dest, 8);
}

static force_inline void
memset_movnt1x4b(char *dest, __m256i ymm)
{
	uint32_t x = m256_get4b(ymm);

	_mm_stream_si32((int *)dest, (int)x);

	VALGRIND_DO_FLUSH(dest, 4);
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
		memset_movnt8x64b(dest, ymm);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_movnt4x64b(dest, ymm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_movnt2x64b(dest, ymm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_movnt1x64b(dest, ymm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len == 0)
		goto end;

	/* There's no point in using more than 1 nt store for 1 cache line. */
	if (util_is_pow2(len)) {
		if (len == 32)
			memset_movnt1x32b(dest, ymm);
		else if (len == 16)
			memset_movnt1x16b(dest, ymm);
		else if (len == 8)
			memset_movnt1x8b(dest, ymm);
		else if (len == 4)
			memset_movnt1x4b(dest, ymm);
		else
			goto nonnt;

		goto end;
	}

nonnt:
	memset_small_avx(dest, ymm, len);
end:
	avx_zeroupper();

	maybe_barrier();
}
