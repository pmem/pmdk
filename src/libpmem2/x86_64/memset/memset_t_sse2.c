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
mm_store_si128(char *dest, unsigned idx, __m128i src)
{
	_mm_store_si128((__m128i *)dest + idx, src);
}

static force_inline void
memset_mov4x64b(char *dest, __m128i xmm, flush64b_fn flush64b)
{
	mm_store_si128(dest, 0, xmm);
	mm_store_si128(dest, 1, xmm);
	mm_store_si128(dest, 2, xmm);
	mm_store_si128(dest, 3, xmm);
	mm_store_si128(dest, 4, xmm);
	mm_store_si128(dest, 5, xmm);
	mm_store_si128(dest, 6, xmm);
	mm_store_si128(dest, 7, xmm);
	mm_store_si128(dest, 8, xmm);
	mm_store_si128(dest, 9, xmm);
	mm_store_si128(dest, 10, xmm);
	mm_store_si128(dest, 11, xmm);
	mm_store_si128(dest, 12, xmm);
	mm_store_si128(dest, 13, xmm);
	mm_store_si128(dest, 14, xmm);
	mm_store_si128(dest, 15, xmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memset_mov2x64b(char *dest, __m128i xmm, flush64b_fn flush64b)
{
	mm_store_si128(dest, 0, xmm);
	mm_store_si128(dest, 1, xmm);
	mm_store_si128(dest, 2, xmm);
	mm_store_si128(dest, 3, xmm);
	mm_store_si128(dest, 4, xmm);
	mm_store_si128(dest, 5, xmm);
	mm_store_si128(dest, 6, xmm);
	mm_store_si128(dest, 7, xmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memset_mov1x64b(char *dest, __m128i xmm, flush64b_fn flush64b)
{
	mm_store_si128(dest, 0, xmm);
	mm_store_si128(dest, 1, xmm);
	mm_store_si128(dest, 2, xmm);
	mm_store_si128(dest, 3, xmm);

	flush64b(dest + 0 * 64);
}

static force_inline void
memset_mov_sse2(char *dest, int c, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	__m128i xmm = _mm_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_sse2(dest, xmm, cnt, flush);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 4 * 64) {
		memset_mov4x64b(dest, xmm, flush64b);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_mov2x64b(dest, xmm, flush64b);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_mov1x64b(dest, xmm, flush64b);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memset_small_sse2(dest, xmm, len, flush);
}

void
memset_mov_sse2_noflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_sse2(dest, c, len, noflush, noflush64b);
}

void
memset_mov_sse2_empty(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_sse2(dest, c, len, flush_empty_nolog, flush64b_empty);
}

void
memset_mov_sse2_clflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_sse2(dest, c, len, flush_clflush_nolog, pmem_clflush);
}

void
memset_mov_sse2_clflushopt(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_sse2(dest, c, len, flush_clflushopt_nolog,
			pmem_clflushopt);
}

void
memset_mov_sse2_clwb(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_sse2(dest, c, len, flush_clwb_nolog, pmem_clwb);
}
