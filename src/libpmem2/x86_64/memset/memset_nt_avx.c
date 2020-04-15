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
mm256_stream_si256(char *dest, unsigned idx, __m256i src)
{
	_mm256_stream_si256((__m256i *)dest + idx, src);
}

static force_inline void
memset_movnt8x64b(char *dest, __m256i ymm)
{
	mm256_stream_si256(dest, 0, ymm);
	mm256_stream_si256(dest, 1, ymm);
	mm256_stream_si256(dest, 2, ymm);
	mm256_stream_si256(dest, 3, ymm);
	mm256_stream_si256(dest, 4, ymm);
	mm256_stream_si256(dest, 5, ymm);
	mm256_stream_si256(dest, 6, ymm);
	mm256_stream_si256(dest, 7, ymm);
	mm256_stream_si256(dest, 8, ymm);
	mm256_stream_si256(dest, 9, ymm);
	mm256_stream_si256(dest, 10, ymm);
	mm256_stream_si256(dest, 11, ymm);
	mm256_stream_si256(dest, 12, ymm);
	mm256_stream_si256(dest, 13, ymm);
	mm256_stream_si256(dest, 14, ymm);
	mm256_stream_si256(dest, 15, ymm);
}

static force_inline void
memset_movnt4x64b(char *dest, __m256i ymm)
{
	mm256_stream_si256(dest, 0, ymm);
	mm256_stream_si256(dest, 1, ymm);
	mm256_stream_si256(dest, 2, ymm);
	mm256_stream_si256(dest, 3, ymm);
	mm256_stream_si256(dest, 4, ymm);
	mm256_stream_si256(dest, 5, ymm);
	mm256_stream_si256(dest, 6, ymm);
	mm256_stream_si256(dest, 7, ymm);
}

static force_inline void
memset_movnt2x64b(char *dest, __m256i ymm)
{
	mm256_stream_si256(dest, 0, ymm);
	mm256_stream_si256(dest, 1, ymm);
	mm256_stream_si256(dest, 2, ymm);
	mm256_stream_si256(dest, 3, ymm);
}

static force_inline void
memset_movnt1x64b(char *dest, __m256i ymm)
{
	mm256_stream_si256(dest, 0, ymm);
	mm256_stream_si256(dest, 1, ymm);
}

static force_inline void
memset_movnt1x32b(char *dest, __m256i ymm)
{
	mm256_stream_si256(dest, 0, ymm);
}

static force_inline void
memset_movnt1x16b(char *dest, __m256i ymm)
{
	__m128i xmm0 = m256_get16b(ymm);

	_mm_stream_si128((__m128i *)dest, xmm0);
}

static force_inline void
memset_movnt1x8b(char *dest, __m256i ymm)
{
	uint64_t x = m256_get8b(ymm);

	_mm_stream_si64((long long *)dest, (long long)x);
}

static force_inline void
memset_movnt1x4b(char *dest, __m256i ymm)
{
	uint32_t x = m256_get4b(ymm);

	_mm_stream_si32((int *)dest, (int)x);
}

static force_inline void
memset_movnt_avx(char *dest, int c, size_t len, flush_fn flush,
		barrier_fn barrier)
{
	char *orig_dest = dest;
	size_t orig_len = len;

	__m256i ymm = _mm256_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_avx(dest, ymm, cnt, flush);

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
	memset_small_avx(dest, ymm, len, flush);
end:
	avx_zeroupper();

	barrier();

	VALGRIND_DO_FLUSH(orig_dest, orig_len);
}

void
memset_movnt_avx_noflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_avx(dest, c, len, noflush, barrier_after_ntstores);
}

void
memset_movnt_avx_empty(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_avx(dest, c, len, flush_empty_nolog,
			barrier_after_ntstores);
}

void
memset_movnt_avx_clflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_avx(dest, c, len, flush_clflush_nolog,
			barrier_after_ntstores);
}

void
memset_movnt_avx_clflushopt(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_avx(dest, c, len, flush_clflushopt_nolog,
			no_barrier_after_ntstores);
}

void
memset_movnt_avx_clwb(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_avx(dest, c, len, flush_clwb_nolog,
			no_barrier_after_ntstores);
}
