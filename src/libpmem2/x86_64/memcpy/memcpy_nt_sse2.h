// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_sse2.h"
#include "valgrind_internal.h"

static force_inline void
memmove_movnt4x64b(char *dest, const char *src)
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

	_mm_stream_si128((__m128i *)dest + 0, xmm0);
	_mm_stream_si128((__m128i *)dest + 1, xmm1);
	_mm_stream_si128((__m128i *)dest + 2, xmm2);
	_mm_stream_si128((__m128i *)dest + 3, xmm3);
	_mm_stream_si128((__m128i *)dest + 4, xmm4);
	_mm_stream_si128((__m128i *)dest + 5, xmm5);
	_mm_stream_si128((__m128i *)dest + 6, xmm6);
	_mm_stream_si128((__m128i *)dest + 7, xmm7);
	_mm_stream_si128((__m128i *)dest + 8, xmm8);
	_mm_stream_si128((__m128i *)dest + 9, xmm9);
	_mm_stream_si128((__m128i *)dest + 10, xmm10);
	_mm_stream_si128((__m128i *)dest + 11, xmm11);
	_mm_stream_si128((__m128i *)dest + 12, xmm12);
	_mm_stream_si128((__m128i *)dest + 13, xmm13);
	_mm_stream_si128((__m128i *)dest + 14, xmm14);
	_mm_stream_si128((__m128i *)dest + 15, xmm15);

	VALGRIND_DO_FLUSH(dest, 4 * 64);
}

static force_inline void
memmove_movnt2x64b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src + 0);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)src + 1);
	__m128i xmm2 = _mm_loadu_si128((__m128i *)src + 2);
	__m128i xmm3 = _mm_loadu_si128((__m128i *)src + 3);
	__m128i xmm4 = _mm_loadu_si128((__m128i *)src + 4);
	__m128i xmm5 = _mm_loadu_si128((__m128i *)src + 5);
	__m128i xmm6 = _mm_loadu_si128((__m128i *)src + 6);
	__m128i xmm7 = _mm_loadu_si128((__m128i *)src + 7);

	_mm_stream_si128((__m128i *)dest + 0, xmm0);
	_mm_stream_si128((__m128i *)dest + 1, xmm1);
	_mm_stream_si128((__m128i *)dest + 2, xmm2);
	_mm_stream_si128((__m128i *)dest + 3, xmm3);
	_mm_stream_si128((__m128i *)dest + 4, xmm4);
	_mm_stream_si128((__m128i *)dest + 5, xmm5);
	_mm_stream_si128((__m128i *)dest + 6, xmm6);
	_mm_stream_si128((__m128i *)dest + 7, xmm7);

	VALGRIND_DO_FLUSH(dest, 2 * 64);
}

static force_inline void
memmove_movnt1x64b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src + 0);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)src + 1);
	__m128i xmm2 = _mm_loadu_si128((__m128i *)src + 2);
	__m128i xmm3 = _mm_loadu_si128((__m128i *)src + 3);

	_mm_stream_si128((__m128i *)dest + 0, xmm0);
	_mm_stream_si128((__m128i *)dest + 1, xmm1);
	_mm_stream_si128((__m128i *)dest + 2, xmm2);
	_mm_stream_si128((__m128i *)dest + 3, xmm3);

	VALGRIND_DO_FLUSH(dest, 64);
}

static force_inline void
memmove_movnt1x32b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src + 0);
	__m128i xmm1 = _mm_loadu_si128((__m128i *)src + 1);

	_mm_stream_si128((__m128i *)dest + 0, xmm0);
	_mm_stream_si128((__m128i *)dest + 1, xmm1);

	VALGRIND_DO_FLUSH(dest, 32);
}

static force_inline void
memmove_movnt1x16b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src);

	_mm_stream_si128((__m128i *)dest, xmm0);

	VALGRIND_DO_FLUSH(dest, 16);
}

static force_inline void
memmove_movnt1x8b(char *dest, const char *src)
{
	_mm_stream_si64((long long *)dest, *(long long *)src);

	VALGRIND_DO_FLUSH(dest, 8);
}

static force_inline void
memmove_movnt1x4b(char *dest, const char *src)
{
	_mm_stream_si32((int *)dest, *(int *)src);

	VALGRIND_DO_FLUSH(dest, 4);
}

static force_inline void
memmove_movnt_sse_fw(char *dest, const char *src, size_t len)
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
		memmove_movnt4x64b(dest, src);
		dest += 4 * 64;
		src += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memmove_movnt2x64b(dest, src);
		dest += 2 * 64;
		src += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memmove_movnt1x64b(dest, src);

		dest += 1 * 64;
		src += 1 * 64;
		len -= 1 * 64;
	}

	if (len == 0)
		return;

	/* There's no point in using more than 1 nt store for 1 cache line. */
	if (util_is_pow2(len)) {
		if (len == 32)
			memmove_movnt1x32b(dest, src);
		else if (len == 16)
			memmove_movnt1x16b(dest, src);
		else if (len == 8)
			memmove_movnt1x8b(dest, src);
		else if (len == 4)
			memmove_movnt1x4b(dest, src);
		else
			goto nonnt;

		return;
	}

nonnt:
	memmove_small_sse2(dest, src, len);
}

static force_inline void
memmove_movnt_sse_bw(char *dest, const char *src, size_t len)
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
		memmove_movnt4x64b(dest, src);
	}

	if (len >= 2 * 64) {
		dest -= 2 * 64;
		src -= 2 * 64;
		len -= 2 * 64;
		memmove_movnt2x64b(dest, src);
	}

	if (len >= 1 * 64) {
		dest -= 1 * 64;
		src -= 1 * 64;
		len -= 1 * 64;
		memmove_movnt1x64b(dest, src);
	}

	if (len == 0)
		return;

	/* There's no point in using more than 1 nt store for 1 cache line. */
	if (util_is_pow2(len)) {
		if (len == 32) {
			dest -= 32;
			src -= 32;
			memmove_movnt1x32b(dest, src);
		} else if (len == 16) {
			dest -= 16;
			src -= 16;
			memmove_movnt1x16b(dest, src);
		} else if (len == 8) {
			dest -= 8;
			src -= 8;
			memmove_movnt1x8b(dest, src);
		} else if (len == 4) {
			dest -= 4;
			src -= 4;
			memmove_movnt1x4b(dest, src);
		} else {
			goto nonnt;
		}

		return;
	}

nonnt:
	dest -= len;
	src -= len;
	memmove_small_sse2(dest, src, len);
}

void
EXPORTED_SYMBOL(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_movnt_sse_fw(dest, src, len);
	else
		memmove_movnt_sse_bw(dest, src, len);

	maybe_barrier();
}
