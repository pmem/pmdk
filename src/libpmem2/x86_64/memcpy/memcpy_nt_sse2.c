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

static force_inline __m128i
mm_loadu_si128(const char *src, unsigned idx)
{
	return _mm_loadu_si128((const __m128i *)src + idx);
}

static force_inline void
mm_stream_si128(char *dest, unsigned idx, __m128i src)
{
	_mm_stream_si128((__m128i *)dest + idx, src);
	barrier();
}

static force_inline void
memmove_movnt4x64b(char *dest, const char *src)
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

	mm_stream_si128(dest, 0, xmm0);
	mm_stream_si128(dest, 1, xmm1);
	mm_stream_si128(dest, 2, xmm2);
	mm_stream_si128(dest, 3, xmm3);
	mm_stream_si128(dest, 4, xmm4);
	mm_stream_si128(dest, 5, xmm5);
	mm_stream_si128(dest, 6, xmm6);
	mm_stream_si128(dest, 7, xmm7);
	mm_stream_si128(dest, 8, xmm8);
	mm_stream_si128(dest, 9, xmm9);
	mm_stream_si128(dest, 10, xmm10);
	mm_stream_si128(dest, 11, xmm11);
	mm_stream_si128(dest, 12, xmm12);
	mm_stream_si128(dest, 13, xmm13);
	mm_stream_si128(dest, 14, xmm14);
	mm_stream_si128(dest, 15, xmm15);
}

static force_inline void
memmove_movnt2x64b(char *dest, const char *src)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);
	__m128i xmm1 = mm_loadu_si128(src, 1);
	__m128i xmm2 = mm_loadu_si128(src, 2);
	__m128i xmm3 = mm_loadu_si128(src, 3);
	__m128i xmm4 = mm_loadu_si128(src, 4);
	__m128i xmm5 = mm_loadu_si128(src, 5);
	__m128i xmm6 = mm_loadu_si128(src, 6);
	__m128i xmm7 = mm_loadu_si128(src, 7);

	mm_stream_si128(dest, 0, xmm0);
	mm_stream_si128(dest, 1, xmm1);
	mm_stream_si128(dest, 2, xmm2);
	mm_stream_si128(dest, 3, xmm3);
	mm_stream_si128(dest, 4, xmm4);
	mm_stream_si128(dest, 5, xmm5);
	mm_stream_si128(dest, 6, xmm6);
	mm_stream_si128(dest, 7, xmm7);
}

static force_inline void
memmove_movnt1x64b(char *dest, const char *src)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);
	__m128i xmm1 = mm_loadu_si128(src, 1);
	__m128i xmm2 = mm_loadu_si128(src, 2);
	__m128i xmm3 = mm_loadu_si128(src, 3);

	mm_stream_si128(dest, 0, xmm0);
	mm_stream_si128(dest, 1, xmm1);
	mm_stream_si128(dest, 2, xmm2);
	mm_stream_si128(dest, 3, xmm3);
}

static force_inline void
memmove_movnt1x32b(char *dest, const char *src)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);
	__m128i xmm1 = mm_loadu_si128(src, 1);

	mm_stream_si128(dest, 0, xmm0);
	mm_stream_si128(dest, 1, xmm1);
}

static force_inline void
memmove_movnt1x16b(char *dest, const char *src)
{
	__m128i xmm0 = mm_loadu_si128(src, 0);

	mm_stream_si128(dest, 0, xmm0);
}

static force_inline void
memmove_movnt1x8b(char *dest, const char *src)
{
	_mm_stream_si64((long long *)dest, *(long long *)src);
}

static force_inline void
memmove_movnt1x4b(char *dest, const char *src)
{
	_mm_stream_si32((int *)dest, *(int *)src);
}

static force_inline void
memmove_movnt_sse_fw(char *dest, const char *src, size_t len, flush_fn flush)
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
	memmove_small_sse2(dest, src, len, flush);
}

static force_inline void
memmove_movnt_sse_bw(char *dest, const char *src, size_t len, flush_fn flush)
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
	memmove_small_sse2(dest, src, len, flush);
}

static force_inline void
memmove_movnt_sse2(char *dest, const char *src, size_t len, flush_fn flush,
		barrier_fn barrier)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_movnt_sse_fw(dest, src, len, flush);
	else
		memmove_movnt_sse_bw(dest, src, len, flush);

	barrier();

	VALGRIND_DO_FLUSH(dest, len);
}

void
memmove_movnt_sse2_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_sse2(dest, src, len, noflush, barrier_after_ntstores);
}

void
memmove_movnt_sse2_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_sse2(dest, src, len, flush_empty_nolog,
			barrier_after_ntstores);
}

void
memmove_movnt_sse2_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_sse2(dest, src, len, flush_clflush_nolog,
			barrier_after_ntstores);
}

void
memmove_movnt_sse2_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_sse2(dest, src, len, flush_clflushopt_nolog,
			no_barrier_after_ntstores);
}

void
memmove_movnt_sse2_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_sse2(dest, src, len, flush_clwb_nolog,
			no_barrier_after_ntstores);
}
