// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memset_movdir64b.h"
#include "out.h"
#include "util.h"
#include "valgrind_internal.h"

static force_inline void
movdir64b(char *dest, const char *src)
{
	_movdir64b(dest, src);
	compiler_barrier();
}

static force_inline void
memset_movnt1x32b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest, ymm);
}

static force_inline void
memset_movnt1x16b(char *dest, __m256i ymm)
{
	__m128i xmm = _mm256_extracti128_si256(ymm, 0);

	_mm_stream_si128((__m128i *)dest, xmm);
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
memset_movnt_movdir64b(char *dest, int c, size_t len, flush_fn flush,
		barrier_fn barrier)
{
	char *orig_dest = dest;
	size_t orig_len = len;

	__m512i zmm = _mm512_set1_epi8((char)c);
	/*
	 * Can't use _mm512_extracti64x4_epi64, because some versions of gcc
	 * crash. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82887
	 */
	__m256i ymm = _mm256_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_movdir64b(dest, ymm, cnt, flush);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 64) {
		movdir64b(dest, (char *)&zmm);
		dest += 64;
		len -= 64;
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
	memset_small_movdir64b(dest, ymm, len, flush);
end:
	avx_zeroupper();

	barrier();

	VALGRIND_DO_FLUSH(orig_dest, orig_len);
}

void
memset_movnt_movdir64b_noflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_movdir64b(dest, c, len, noflush, barrier_after_ntstores);
}

void
memset_movnt_movdir64b_empty(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_movdir64b(dest, c, len, flush_empty_nolog,
			barrier_after_ntstores);
}

void
memset_movnt_movdir64b_clflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_movdir64b(dest, c, len, flush_clflush_nolog,
			barrier_after_ntstores);
}

void
memset_movnt_movdir64b_clflushopt(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_movdir64b(dest, c, len, flush_clflushopt_nolog,
			no_barrier_after_ntstores);
}

void
memset_movnt_movdir64b_clwb(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_movnt_movdir64b(dest, c, len, flush_clwb_nolog,
			no_barrier_after_ntstores);
}
