// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_sse2.h"
#include "out.h"

static force_inline __m128i
mm_loadu_si128(const char *src, unsigned idx)
{
	return _mm_loadu_si128((const __m128i *)src + idx);
}

static force_inline void
mm_store_si128(char *dest, unsigned idx, __m128i src)
{
	_mm_store_si128((__m128i *)dest + idx, src);
}

static force_inline void
memmove_mov4x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);
	__m128i xmm1 = mm_loadu_si128(src, 1);
	__m128i xmm2 = mm_loadu_si128(src, 2);
	__m128i xmm3 = mm_loadu_si128(src, 3);
	__m128i xmm4 = mm_loadu_si128(src, 4);
	__m128i xmm5 = mm_loadu_si128(src, 5);
	__m128i xmm6 = mm_loadu_si128(src, 6);
	__m128i xmm7 = mm_loadu_si128(src, 7);
	__m128i xmm8 = mm_loadu_si128(src, 8);
	__m128i xmm9 = mm_loadu_si128(src, 9);
	__m128i xmm10 = mm_loadu_si128(src, 10);
	__m128i xmm11 = mm_loadu_si128(src, 11);
	__m128i xmm12 = mm_loadu_si128(src, 12);
	__m128i xmm13 = mm_loadu_si128(src, 13);
	__m128i xmm14 = mm_loadu_si128(src, 14);
	__m128i xmm15 = mm_loadu_si128(src, 15);

	mm_store_si128(dest, 0, xmm0);
	mm_store_si128(dest, 1, xmm1);
	mm_store_si128(dest, 2, xmm2);
	mm_store_si128(dest, 3, xmm3);
	mm_store_si128(dest, 4, xmm4);
	mm_store_si128(dest, 5, xmm5);
	mm_store_si128(dest, 6, xmm6);
	mm_store_si128(dest, 7, xmm7);
	mm_store_si128(dest, 8, xmm8);
	mm_store_si128(dest, 9, xmm9);
	mm_store_si128(dest, 10, xmm10);
	mm_store_si128(dest, 11, xmm11);
	mm_store_si128(dest, 12, xmm12);
	mm_store_si128(dest, 13, xmm13);
	mm_store_si128(dest, 14, xmm14);
	mm_store_si128(dest, 15, xmm15);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memmove_mov2x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);
	__m128i xmm1 = mm_loadu_si128(src, 1);
	__m128i xmm2 = mm_loadu_si128(src, 2);
	__m128i xmm3 = mm_loadu_si128(src, 3);
	__m128i xmm4 = mm_loadu_si128(src, 4);
	__m128i xmm5 = mm_loadu_si128(src, 5);
	__m128i xmm6 = mm_loadu_si128(src, 6);
	__m128i xmm7 = mm_loadu_si128(src, 7);

	mm_store_si128(dest, 0, xmm0);
	mm_store_si128(dest, 1, xmm1);
	mm_store_si128(dest, 2, xmm2);
	mm_store_si128(dest, 3, xmm3);
	mm_store_si128(dest, 4, xmm4);
	mm_store_si128(dest, 5, xmm5);
	mm_store_si128(dest, 6, xmm6);
	mm_store_si128(dest, 7, xmm7);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memmove_mov1x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);
	__m128i xmm1 = mm_loadu_si128(src, 1);
	__m128i xmm2 = mm_loadu_si128(src, 2);
	__m128i xmm3 = mm_loadu_si128(src, 3);

	mm_store_si128(dest, 0, xmm0);
	mm_store_si128(dest, 1, xmm1);
	mm_store_si128(dest, 2, xmm2);
	mm_store_si128(dest, 3, xmm3);

	flush64b(dest + 0 * 64);
}

static force_inline void
memmove_mov_sse_fw(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memmove_small_sse2(dest, src, cnt, flush);

		dest += cnt;
		src += cnt;
		len -= cnt;
	}

	while (len >= 4 * 64) {
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
		memmove_small_sse2(dest, src, len, flush);
}

static force_inline void
memmove_mov_sse_bw(char *dest, const char *src, size_t len,
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
		memmove_small_sse2(dest, src, cnt, flush);
	}

	while (len >= 4 * 64) {
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
		memmove_small_sse2(dest - len, src - len, len, flush);
}

static force_inline void
memmove_mov_sse2(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_mov_sse_fw(dest, src, len, flush, flush64b);
	else
		memmove_mov_sse_bw(dest, src, len, flush, flush64b);
}

void
memmove_mov_sse2_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_sse2(dest, src, len, noflush, noflush64b);
}

void
memmove_mov_sse2_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_sse2(dest, src, len, flush_empty_nolog, flush64b_empty);
}

void
memmove_mov_sse2_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_sse2(dest, src, len, flush_clflush_nolog, pmem_clflush);
}

void
memmove_mov_sse2_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_sse2(dest, src, len, flush_clflushopt_nolog,
			pmem_clflushopt);
}

void
memmove_mov_sse2_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_sse2(dest, src, len, flush_clwb_nolog, pmem_clwb);
}
