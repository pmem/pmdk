// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2023, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memset_avx512f.h"

static force_inline void
mm512_store_si512(char *dest, unsigned idx, __m512i src)
{
	_mm512_store_si512((__m512i *)dest + idx, src);
}

static force_inline void
memset_mov32x64b(char *dest, __m512i zmm, flush64b_fn flush64b)
{
	mm512_store_si512(dest, 0, zmm);
	mm512_store_si512(dest, 1, zmm);
	mm512_store_si512(dest, 2, zmm);
	mm512_store_si512(dest, 3, zmm);
	mm512_store_si512(dest, 4, zmm);
	mm512_store_si512(dest, 5, zmm);
	mm512_store_si512(dest, 6, zmm);
	mm512_store_si512(dest, 7, zmm);
	mm512_store_si512(dest, 8, zmm);
	mm512_store_si512(dest, 9, zmm);
	mm512_store_si512(dest, 10, zmm);
	mm512_store_si512(dest, 11, zmm);
	mm512_store_si512(dest, 12, zmm);
	mm512_store_si512(dest, 13, zmm);
	mm512_store_si512(dest, 14, zmm);
	mm512_store_si512(dest, 15, zmm);
	mm512_store_si512(dest, 16, zmm);
	mm512_store_si512(dest, 17, zmm);
	mm512_store_si512(dest, 18, zmm);
	mm512_store_si512(dest, 19, zmm);
	mm512_store_si512(dest, 20, zmm);
	mm512_store_si512(dest, 21, zmm);
	mm512_store_si512(dest, 22, zmm);
	mm512_store_si512(dest, 23, zmm);
	mm512_store_si512(dest, 24, zmm);
	mm512_store_si512(dest, 25, zmm);
	mm512_store_si512(dest, 26, zmm);
	mm512_store_si512(dest, 27, zmm);
	mm512_store_si512(dest, 28, zmm);
	mm512_store_si512(dest, 29, zmm);
	mm512_store_si512(dest, 30, zmm);
	mm512_store_si512(dest, 31, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
	flush64b(dest + 4 * 64);
	flush64b(dest + 5 * 64);
	flush64b(dest + 6 * 64);
	flush64b(dest + 7 * 64);
	flush64b(dest + 8 * 64);
	flush64b(dest + 9 * 64);
	flush64b(dest + 10 * 64);
	flush64b(dest + 11 * 64);
	flush64b(dest + 12 * 64);
	flush64b(dest + 13 * 64);
	flush64b(dest + 14 * 64);
	flush64b(dest + 15 * 64);
	flush64b(dest + 16 * 64);
	flush64b(dest + 17 * 64);
	flush64b(dest + 18 * 64);
	flush64b(dest + 19 * 64);
	flush64b(dest + 20 * 64);
	flush64b(dest + 21 * 64);
	flush64b(dest + 22 * 64);
	flush64b(dest + 23 * 64);
	flush64b(dest + 24 * 64);
	flush64b(dest + 25 * 64);
	flush64b(dest + 26 * 64);
	flush64b(dest + 27 * 64);
	flush64b(dest + 28 * 64);
	flush64b(dest + 29 * 64);
	flush64b(dest + 30 * 64);
	flush64b(dest + 31 * 64);
}

static force_inline void
memset_mov16x64b(char *dest, __m512i zmm, flush64b_fn flush64b)
{
	mm512_store_si512(dest, 0, zmm);
	mm512_store_si512(dest, 1, zmm);
	mm512_store_si512(dest, 2, zmm);
	mm512_store_si512(dest, 3, zmm);
	mm512_store_si512(dest, 4, zmm);
	mm512_store_si512(dest, 5, zmm);
	mm512_store_si512(dest, 6, zmm);
	mm512_store_si512(dest, 7, zmm);
	mm512_store_si512(dest, 8, zmm);
	mm512_store_si512(dest, 9, zmm);
	mm512_store_si512(dest, 10, zmm);
	mm512_store_si512(dest, 11, zmm);
	mm512_store_si512(dest, 12, zmm);
	mm512_store_si512(dest, 13, zmm);
	mm512_store_si512(dest, 14, zmm);
	mm512_store_si512(dest, 15, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
	flush64b(dest + 4 * 64);
	flush64b(dest + 5 * 64);
	flush64b(dest + 6 * 64);
	flush64b(dest + 7 * 64);
	flush64b(dest + 8 * 64);
	flush64b(dest + 9 * 64);
	flush64b(dest + 10 * 64);
	flush64b(dest + 11 * 64);
	flush64b(dest + 12 * 64);
	flush64b(dest + 13 * 64);
	flush64b(dest + 14 * 64);
	flush64b(dest + 15 * 64);
}

static force_inline void
memset_mov8x64b(char *dest, __m512i zmm, flush64b_fn flush64b)
{
	mm512_store_si512(dest, 0, zmm);
	mm512_store_si512(dest, 1, zmm);
	mm512_store_si512(dest, 2, zmm);
	mm512_store_si512(dest, 3, zmm);
	mm512_store_si512(dest, 4, zmm);
	mm512_store_si512(dest, 5, zmm);
	mm512_store_si512(dest, 6, zmm);
	mm512_store_si512(dest, 7, zmm);

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
memset_mov4x64b(char *dest, __m512i zmm, flush64b_fn flush64b)
{
	mm512_store_si512(dest, 0, zmm);
	mm512_store_si512(dest, 1, zmm);
	mm512_store_si512(dest, 2, zmm);
	mm512_store_si512(dest, 3, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memset_mov2x64b(char *dest, __m512i zmm, flush64b_fn flush64b)
{
	mm512_store_si512(dest, 0, zmm);
	mm512_store_si512(dest, 1, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memset_mov1x64b(char *dest, __m512i zmm, flush64b_fn flush64b)
{
	mm512_store_si512(dest, 0, zmm);

	flush64b(dest + 0 * 64);
}

static void
memset_mov_avx512f(char *dest, int c, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	__m512i zmm = _mm512_set1_epi8((char)c);
	/* See comment in memset_movnt_avx512f */
	__m256i ymm = _mm256_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_avx512f(dest, ymm, cnt, flush);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 32 * 64) {
		memset_mov32x64b(dest, zmm, flush64b);
		dest += 32 * 64;
		len -= 32 * 64;
	}

	if (len >= 16 * 64) {
		memset_mov16x64b(dest, zmm, flush64b);
		dest += 16 * 64;
		len -= 16 * 64;
	}

	if (len >= 8 * 64) {
		memset_mov8x64b(dest, zmm, flush64b);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_mov4x64b(dest, zmm, flush64b);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_mov2x64b(dest, zmm, flush64b);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_mov1x64b(dest, zmm, flush64b);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memset_small_avx512f(dest, ymm, len, flush);

	avx_zeroupper();
}

void
memset_mov_avx512f_noflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_avx512f(dest, c, len, noflush, noflush64b);
}

void
memset_mov_avx512f_empty(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_avx512f(dest, c, len, flush_empty_nolog, flush64b_empty);
}

void
memset_mov_avx512f_clflush(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_avx512f(dest, c, len, flush_clflush_nolog, pmem_clflush);
}

void
memset_mov_avx512f_clflushopt(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_avx512f(dest, c, len, flush_clflushopt_nolog,
			pmem_clflushopt);
}

void
memset_mov_avx512f_clwb(char *dest, int c, size_t len)
{
	LOG(15, "dest %p c %d len %zu", dest, c, len);

	memset_mov_avx512f(dest, c, len, flush_clwb_nolog, pmem_clwb);
}
