// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_avx512f.h"

static force_inline __m512i
mm512_loadu_si512(const char *src, unsigned idx)
{
	return _mm512_loadu_si512((const __m512i *)src + idx);
}

static force_inline void
mm512_store_si512(char *dest, unsigned idx, __m512i src)
{
	_mm512_store_si512((__m512i *)dest + idx, src);
}

static force_inline void
memmove_mov32x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);
	__m512i zmm2 = mm512_loadu_si512(src, 2);
	__m512i zmm3 = mm512_loadu_si512(src, 3);
	__m512i zmm4 = mm512_loadu_si512(src, 4);
	__m512i zmm5 = mm512_loadu_si512(src, 5);
	__m512i zmm6 = mm512_loadu_si512(src, 6);
	__m512i zmm7 = mm512_loadu_si512(src, 7);
	__m512i zmm8 = mm512_loadu_si512(src, 8);
	__m512i zmm9 = mm512_loadu_si512(src, 9);
	__m512i zmm10 = mm512_loadu_si512(src, 10);
	__m512i zmm11 = mm512_loadu_si512(src, 11);
	__m512i zmm12 = mm512_loadu_si512(src, 12);
	__m512i zmm13 = mm512_loadu_si512(src, 13);
	__m512i zmm14 = mm512_loadu_si512(src, 14);
	__m512i zmm15 = mm512_loadu_si512(src, 15);
	__m512i zmm16 = mm512_loadu_si512(src, 16);
	__m512i zmm17 = mm512_loadu_si512(src, 17);
	__m512i zmm18 = mm512_loadu_si512(src, 18);
	__m512i zmm19 = mm512_loadu_si512(src, 19);
	__m512i zmm20 = mm512_loadu_si512(src, 20);
	__m512i zmm21 = mm512_loadu_si512(src, 21);
	__m512i zmm22 = mm512_loadu_si512(src, 22);
	__m512i zmm23 = mm512_loadu_si512(src, 23);
	__m512i zmm24 = mm512_loadu_si512(src, 24);
	__m512i zmm25 = mm512_loadu_si512(src, 25);
	__m512i zmm26 = mm512_loadu_si512(src, 26);
	__m512i zmm27 = mm512_loadu_si512(src, 27);
	__m512i zmm28 = mm512_loadu_si512(src, 28);
	__m512i zmm29 = mm512_loadu_si512(src, 29);
	__m512i zmm30 = mm512_loadu_si512(src, 30);
	__m512i zmm31 = mm512_loadu_si512(src, 31);

	mm512_store_si512(dest, 0, zmm0);
	mm512_store_si512(dest, 1, zmm1);
	mm512_store_si512(dest, 2, zmm2);
	mm512_store_si512(dest, 3, zmm3);
	mm512_store_si512(dest, 4, zmm4);
	mm512_store_si512(dest, 5, zmm5);
	mm512_store_si512(dest, 6, zmm6);
	mm512_store_si512(dest, 7, zmm7);
	mm512_store_si512(dest, 8, zmm8);
	mm512_store_si512(dest, 9, zmm9);
	mm512_store_si512(dest, 10, zmm10);
	mm512_store_si512(dest, 11, zmm11);
	mm512_store_si512(dest, 12, zmm12);
	mm512_store_si512(dest, 13, zmm13);
	mm512_store_si512(dest, 14, zmm14);
	mm512_store_si512(dest, 15, zmm15);
	mm512_store_si512(dest, 16, zmm16);
	mm512_store_si512(dest, 17, zmm17);
	mm512_store_si512(dest, 18, zmm18);
	mm512_store_si512(dest, 19, zmm19);
	mm512_store_si512(dest, 20, zmm20);
	mm512_store_si512(dest, 21, zmm21);
	mm512_store_si512(dest, 22, zmm22);
	mm512_store_si512(dest, 23, zmm23);
	mm512_store_si512(dest, 24, zmm24);
	mm512_store_si512(dest, 25, zmm25);
	mm512_store_si512(dest, 26, zmm26);
	mm512_store_si512(dest, 27, zmm27);
	mm512_store_si512(dest, 28, zmm28);
	mm512_store_si512(dest, 29, zmm29);
	mm512_store_si512(dest, 30, zmm30);
	mm512_store_si512(dest, 31, zmm31);

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
memmove_mov16x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);
	__m512i zmm2 = mm512_loadu_si512(src, 2);
	__m512i zmm3 = mm512_loadu_si512(src, 3);
	__m512i zmm4 = mm512_loadu_si512(src, 4);
	__m512i zmm5 = mm512_loadu_si512(src, 5);
	__m512i zmm6 = mm512_loadu_si512(src, 6);
	__m512i zmm7 = mm512_loadu_si512(src, 7);
	__m512i zmm8 = mm512_loadu_si512(src, 8);
	__m512i zmm9 = mm512_loadu_si512(src, 9);
	__m512i zmm10 = mm512_loadu_si512(src, 10);
	__m512i zmm11 = mm512_loadu_si512(src, 11);
	__m512i zmm12 = mm512_loadu_si512(src, 12);
	__m512i zmm13 = mm512_loadu_si512(src, 13);
	__m512i zmm14 = mm512_loadu_si512(src, 14);
	__m512i zmm15 = mm512_loadu_si512(src, 15);

	mm512_store_si512(dest, 0, zmm0);
	mm512_store_si512(dest, 1, zmm1);
	mm512_store_si512(dest, 2, zmm2);
	mm512_store_si512(dest, 3, zmm3);
	mm512_store_si512(dest, 4, zmm4);
	mm512_store_si512(dest, 5, zmm5);
	mm512_store_si512(dest, 6, zmm6);
	mm512_store_si512(dest, 7, zmm7);
	mm512_store_si512(dest, 8, zmm8);
	mm512_store_si512(dest, 9, zmm9);
	mm512_store_si512(dest, 10, zmm10);
	mm512_store_si512(dest, 11, zmm11);
	mm512_store_si512(dest, 12, zmm12);
	mm512_store_si512(dest, 13, zmm13);
	mm512_store_si512(dest, 14, zmm14);
	mm512_store_si512(dest, 15, zmm15);

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
memmove_mov8x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);
	__m512i zmm2 = mm512_loadu_si512(src, 2);
	__m512i zmm3 = mm512_loadu_si512(src, 3);
	__m512i zmm4 = mm512_loadu_si512(src, 4);
	__m512i zmm5 = mm512_loadu_si512(src, 5);
	__m512i zmm6 = mm512_loadu_si512(src, 6);
	__m512i zmm7 = mm512_loadu_si512(src, 7);

	mm512_store_si512(dest, 0, zmm0);
	mm512_store_si512(dest, 1, zmm1);
	mm512_store_si512(dest, 2, zmm2);
	mm512_store_si512(dest, 3, zmm3);
	mm512_store_si512(dest, 4, zmm4);
	mm512_store_si512(dest, 5, zmm5);
	mm512_store_si512(dest, 6, zmm6);
	mm512_store_si512(dest, 7, zmm7);

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
memmove_mov4x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);
	__m512i zmm2 = mm512_loadu_si512(src, 2);
	__m512i zmm3 = mm512_loadu_si512(src, 3);

	mm512_store_si512(dest, 0, zmm0);
	mm512_store_si512(dest, 1, zmm1);
	mm512_store_si512(dest, 2, zmm2);
	mm512_store_si512(dest, 3, zmm3);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memmove_mov2x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);

	mm512_store_si512(dest, 0, zmm0);
	mm512_store_si512(dest, 1, zmm1);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memmove_mov1x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);

	mm512_store_si512(dest, 0, zmm0);

	flush64b(dest + 0 * 64);
}

static force_inline void
memmove_mov_avx512f_fw(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memmove_small_avx512f(dest, src, cnt, flush);

		dest += cnt;
		src += cnt;
		len -= cnt;
	}

	while (len >= 32 * 64) {
		memmove_mov32x64b(dest, src, flush64b);
		dest += 32 * 64;
		src += 32 * 64;
		len -= 32 * 64;
	}

	if (len >= 16 * 64) {
		memmove_mov16x64b(dest, src, flush64b);
		dest += 16 * 64;
		src += 16 * 64;
		len -= 16 * 64;
	}

	if (len >= 8 * 64) {
		memmove_mov8x64b(dest, src, flush64b);
		dest += 8 * 64;
		src += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memmove_mov4x64b(dest, src, flush64b);
		dest += 4 * 64;
		src += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memmove_mov2x64b(dest, src, flush64b);
		dest += 2 * 64;
		src += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memmove_mov1x64b(dest, src, flush64b);

		dest += 1 * 64;
		src += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memmove_small_avx512f(dest, src, len, flush);
}

static force_inline void
memmove_mov_avx512f_bw(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	dest += len;
	src += len;

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		if (cnt > len)
			cnt = len;

		dest -= cnt;
		src -= cnt;
		len -= cnt;

		memmove_small_avx512f(dest, src, cnt, flush);
	}

	while (len >= 32 * 64) {
		dest -= 32 * 64;
		src -= 32 * 64;
		len -= 32 * 64;
		memmove_mov32x64b(dest, src, flush64b);
	}

	if (len >= 16 * 64) {
		dest -= 16 * 64;
		src -= 16 * 64;
		len -= 16 * 64;
		memmove_mov16x64b(dest, src, flush64b);
	}

	if (len >= 8 * 64) {
		dest -= 8 * 64;
		src -= 8 * 64;
		len -= 8 * 64;
		memmove_mov8x64b(dest, src, flush64b);
	}

	if (len >= 4 * 64) {
		dest -= 4 * 64;
		src -= 4 * 64;
		len -= 4 * 64;
		memmove_mov4x64b(dest, src, flush64b);
	}

	if (len >= 2 * 64) {
		dest -= 2 * 64;
		src -= 2 * 64;
		len -= 2 * 64;
		memmove_mov2x64b(dest, src, flush64b);
	}

	if (len >= 1 * 64) {
		dest -= 1 * 64;
		src -= 1 * 64;
		len -= 1 * 64;
		memmove_mov1x64b(dest, src, flush64b);
	}

	if (len)
		memmove_small_avx512f(dest - len, src - len, len, flush);
}

static force_inline void
memmove_mov_avx512f(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_mov_avx512f_fw(dest, src, len, flush, flush64b);
	else
		memmove_mov_avx512f_bw(dest, src, len, flush, flush64b);

	avx_zeroupper();
}

void
memmove_mov_avx512f_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f(dest, src, len, noflush, noflush64b);
}

void
memmove_mov_avx512f_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f(dest, src, len, flush_empty_nolog, flush64b_empty);
}

void
memmove_mov_avx512f_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f(dest, src, len, flush_clflush_nolog, pmem_clflush);
}

void
memmove_mov_avx512f_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f(dest, src, len, flush_clflushopt_nolog,
			pmem_clflushopt);
}

void
memmove_mov_avx512f_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx512f(dest, src, len, flush_clwb_nolog, pmem_clwb);
}
