// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2019, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_sse2.h"

static force_inline void
memmove_mov4x64b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src + 0);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)src + 1);
	__m128i xmm2 = _mm_loadu_si128((__m128i *)src + 2);
	__m128i xmm3 = _mm_loadu_si128((__m128i *)src + 3);
	__m128i xmm4 = _mm_loadu_si128((__m128i *)src + 4);
	__m128i xmm5 = _mm_loadu_si128((__m128i *)src + 5);
	__m128i xmm6 = _mm_loadu_si128((__m128i *)src + 6);
	__m128i xmm7 = _mm_loadu_si128((__m128i *)src + 7);
	__m128i xmm8 = _mm_loadu_si128((__m128i *)src + 8);
	__m128i xmm9 = _mm_loadu_si128((__m128i *)src + 9);
	__m128i xmm10 = _mm_loadu_si128((__m128i *)src + 10);
	__m128i xmm11 = _mm_loadu_si128((__m128i *)src + 11);
	__m128i xmm12 = _mm_loadu_si128((__m128i *)src + 12);
	__m128i xmm13 = _mm_loadu_si128((__m128i *)src + 13);
	__m128i xmm14 = _mm_loadu_si128((__m128i *)src + 14);
	__m128i xmm15 = _mm_loadu_si128((__m128i *)src + 15);

	_mm_store_si128((__m128i *)dest + 0, xmm0);
	_mm_store_si128((__m128i *)dest + 1, xmm1);
	_mm_store_si128((__m128i *)dest + 2, xmm2);
	_mm_store_si128((__m128i *)dest + 3, xmm3);
	_mm_store_si128((__m128i *)dest + 4, xmm4);
	_mm_store_si128((__m128i *)dest + 5, xmm5);
	_mm_store_si128((__m128i *)dest + 6, xmm6);
	_mm_store_si128((__m128i *)dest + 7, xmm7);
	_mm_store_si128((__m128i *)dest + 8, xmm8);
	_mm_store_si128((__m128i *)dest + 9, xmm9);
	_mm_store_si128((__m128i *)dest + 10, xmm10);
	_mm_store_si128((__m128i *)dest + 11, xmm11);
	_mm_store_si128((__m128i *)dest + 12, xmm12);
	_mm_store_si128((__m128i *)dest + 13, xmm13);
	_mm_store_si128((__m128i *)dest + 14, xmm14);
	_mm_store_si128((__m128i *)dest + 15, xmm15);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memmove_mov2x64b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src + 0);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)src + 1);
	__m128i xmm2 = _mm_loadu_si128((__m128i *)src + 2);
	__m128i xmm3 = _mm_loadu_si128((__m128i *)src + 3);
	__m128i xmm4 = _mm_loadu_si128((__m128i *)src + 4);
	__m128i xmm5 = _mm_loadu_si128((__m128i *)src + 5);
	__m128i xmm6 = _mm_loadu_si128((__m128i *)src + 6);
	__m128i xmm7 = _mm_loadu_si128((__m128i *)src + 7);

	_mm_store_si128((__m128i *)dest + 0, xmm0);
	_mm_store_si128((__m128i *)dest + 1, xmm1);
	_mm_store_si128((__m128i *)dest + 2, xmm2);
	_mm_store_si128((__m128i *)dest + 3, xmm3);
	_mm_store_si128((__m128i *)dest + 4, xmm4);
	_mm_store_si128((__m128i *)dest + 5, xmm5);
	_mm_store_si128((__m128i *)dest + 6, xmm6);
	_mm_store_si128((__m128i *)dest + 7, xmm7);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memmove_mov1x64b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src + 0);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)src + 1);
	__m128i xmm2 = _mm_loadu_si128((__m128i *)src + 2);
	__m128i xmm3 = _mm_loadu_si128((__m128i *)src + 3);

	_mm_store_si128((__m128i *)dest + 0, xmm0);
	_mm_store_si128((__m128i *)dest + 1, xmm1);
	_mm_store_si128((__m128i *)dest + 2, xmm2);
	_mm_store_si128((__m128i *)dest + 3, xmm3);

	flush64b(dest + 0 * 64);
}

static force_inline void
memmove_mov_sse_fw(char *dest, const char *src, size_t len)
{
	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memmove_small_sse2(dest, src, cnt);

		dest += cnt;
		src += cnt;
		len -= cnt;
	}

	while (len >= 4 * 64) {
		memmove_mov4x64b(dest, src);
		dest += 4 * 64;
		src += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memmove_mov2x64b(dest, src);
		dest += 2 * 64;
		src += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memmove_mov1x64b(dest, src);

		dest += 1 * 64;
		src += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memmove_small_sse2(dest, src, len);
}

static force_inline void
memmove_mov_sse_bw(char *dest, const char *src, size_t len)
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
		memmove_small_sse2(dest, src, cnt);
	}

	while (len >= 4 * 64) {
		dest -= 4 * 64;
		src -= 4 * 64;
		len -= 4 * 64;
		memmove_mov4x64b(dest, src);
	}

	if (len >= 2 * 64) {
		dest -= 2 * 64;
		src -= 2 * 64;
		len -= 2 * 64;
		memmove_mov2x64b(dest, src);
	}

	if (len >= 1 * 64) {
		dest -= 1 * 64;
		src -= 1 * 64;
		len -= 1 * 64;
		memmove_mov1x64b(dest, src);
	}

	if (len)
		memmove_small_sse2(dest - len, src - len, len);
}

void
EXPORTED_SYMBOL(char *dest, const char *src, size_t len)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_mov_sse_fw(dest, src, len);
	else
		memmove_mov_sse_bw(dest, src, len);
}
