// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2020, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_avx.h"
#include "valgrind_internal.h"

static force_inline __m256i
mm256_loadu_si256(const char *src, unsigned idx)
{
	return _mm256_loadu_si256((const __m256i *)src + idx);
}

static force_inline void
mm256_stream_si256(char *dest, unsigned idx, __m256i src)
{
	_mm256_stream_si256((__m256i *)dest + idx, src);
	barrier();
}

static force_inline void
memmove_movnt8x64b(char *dest, const char *src)
{
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);
	__m256i ymm2 = mm256_loadu_si256(src, 2);
	__m256i ymm3 = mm256_loadu_si256(src, 3);
	__m256i ymm4 = mm256_loadu_si256(src, 4);
	__m256i ymm5 = mm256_loadu_si256(src, 5);
	__m256i ymm6 = mm256_loadu_si256(src, 6);
	__m256i ymm7 = mm256_loadu_si256(src, 7);
	__m256i ymm8 = mm256_loadu_si256(src, 8);
	__m256i ymm9 = mm256_loadu_si256(src, 9);
	__m256i ymm10 = mm256_loadu_si256(src, 10);
	__m256i ymm11 = mm256_loadu_si256(src, 11);
	__m256i ymm12 = mm256_loadu_si256(src, 12);
	__m256i ymm13 = mm256_loadu_si256(src, 13);
	__m256i ymm14 = mm256_loadu_si256(src, 14);
	__m256i ymm15 = mm256_loadu_si256(src, 15);

	mm256_stream_si256(dest, 0, ymm0);
	mm256_stream_si256(dest, 1, ymm1);
	mm256_stream_si256(dest, 2, ymm2);
	mm256_stream_si256(dest, 3, ymm3);
	mm256_stream_si256(dest, 4, ymm4);
	mm256_stream_si256(dest, 5, ymm5);
	mm256_stream_si256(dest, 6, ymm6);
	mm256_stream_si256(dest, 7, ymm7);
	mm256_stream_si256(dest, 8, ymm8);
	mm256_stream_si256(dest, 9, ymm9);
	mm256_stream_si256(dest, 10, ymm10);
	mm256_stream_si256(dest, 11, ymm11);
	mm256_stream_si256(dest, 12, ymm12);
	mm256_stream_si256(dest, 13, ymm13);
	mm256_stream_si256(dest, 14, ymm14);
	mm256_stream_si256(dest, 15, ymm15);
}

static force_inline void
memmove_movnt4x64b(char *dest, const char *src)
{
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);
	__m256i ymm2 = mm256_loadu_si256(src, 2);
	__m256i ymm3 = mm256_loadu_si256(src, 3);
	__m256i ymm4 = mm256_loadu_si256(src, 4);
	__m256i ymm5 = mm256_loadu_si256(src, 5);
	__m256i ymm6 = mm256_loadu_si256(src, 6);
	__m256i ymm7 = mm256_loadu_si256(src, 7);

	mm256_stream_si256(dest, 0, ymm0);
	mm256_stream_si256(dest, 1, ymm1);
	mm256_stream_si256(dest, 2, ymm2);
	mm256_stream_si256(dest, 3, ymm3);
	mm256_stream_si256(dest, 4, ymm4);
	mm256_stream_si256(dest, 5, ymm5);
	mm256_stream_si256(dest, 6, ymm6);
	mm256_stream_si256(dest, 7, ymm7);
}

static force_inline void
memmove_movnt2x64b(char *dest, const char *src)
{
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);
	__m256i ymm2 = mm256_loadu_si256(src, 2);
	__m256i ymm3 = mm256_loadu_si256(src, 3);

	mm256_stream_si256(dest, 0, ymm0);
	mm256_stream_si256(dest, 1, ymm1);
	mm256_stream_si256(dest, 2, ymm2);
	mm256_stream_si256(dest, 3, ymm3);
}

static force_inline void
memmove_movnt1x64b(char *dest, const char *src)
{
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);

	mm256_stream_si256(dest, 0, ymm0);
	mm256_stream_si256(dest, 1, ymm1);
}

static force_inline void
memmove_movnt1x32b(char *dest, const char *src)
{
	__m256i ymm0 = _mm256_loadu_si256((__m256i *)src);

	mm256_stream_si256(dest, 0, ymm0);
}

static force_inline void
memmove_movnt1x16b(char *dest, const char *src)
{
	__m128i xmm0 = _mm_loadu_si128((__m128i *)src);

	_mm_stream_si128((__m128i *)dest, xmm0);
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
memmove_movnt_avx_fw(char *dest, const char *src, size_t len, flush_fn flush,
		perf_barrier_fn perf_barrier)
{
	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memmove_small_avx(dest, src, cnt, flush);

		dest += cnt;
		src += cnt;
		len -= cnt;
	}

	while (len >= 12 * 64) {
		memmove_movnt8x64b(dest, src);
		dest += 8 * 64;
		src += 8 * 64;
		len -= 8 * 64;

		memmove_movnt4x64b(dest, src);
		dest += 4 * 64;
		src += 4 * 64;
		len -= 4 * 64;

		if (len)
			perf_barrier();
	}

	if (len >= 8 * 64) {
		memmove_movnt8x64b(dest, src);
		dest += 8 * 64;
		src += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
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
		goto end;

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

		goto end;
	}

nonnt:
	memmove_small_avx(dest, src, len, flush);
end:
	avx_zeroupper();
}

static force_inline void
memmove_movnt_avx_bw(char *dest, const char *src, size_t len, flush_fn flush,
		perf_barrier_fn perf_barrier)
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

		memmove_small_avx(dest, src, cnt, flush);
	}

	while (len >= 12 * 64) {
		dest -= 8 * 64;
		src -= 8 * 64;
		len -= 8 * 64;
		memmove_movnt8x64b(dest, src);

		dest -= 4 * 64;
		src -= 4 * 64;
		len -= 4 * 64;
		memmove_movnt4x64b(dest, src);

		if (len)
			perf_barrier();
	}

	if (len >= 8 * 64) {
		dest -= 8 * 64;
		src -= 8 * 64;
		len -= 8 * 64;
		memmove_movnt8x64b(dest, src);
	}

	if (len >= 4 * 64) {
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
		goto end;

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

		goto end;
	}

nonnt:
	dest -= len;
	src -= len;
	memmove_small_avx(dest, src, len, flush);
end:
	avx_zeroupper();
}

static force_inline void
memmove_movnt_avx(char *dest, const char *src, size_t len, flush_fn flush,
		barrier_fn barrier, perf_barrier_fn perf_barrier)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_movnt_avx_fw(dest, src, len, flush, perf_barrier);
	else
		memmove_movnt_avx_bw(dest, src, len, flush, perf_barrier);

	barrier();

	VALGRIND_DO_FLUSH(dest, len);
}

void
memmove_movnt_avx_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx(dest, src, len, noflush, barrier_after_ntstores,
			wc_barrier);
}

void
memmove_movnt_avx_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx(dest, src, len, flush_empty_nolog,
			barrier_after_ntstores, wc_barrier);
}
void
memmove_movnt_avx_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx(dest, src, len, flush_clflush_nolog,
			barrier_after_ntstores, wc_barrier);
}

void
memmove_movnt_avx_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx(dest, src, len, flush_clflushopt_nolog,
			no_barrier_after_ntstores, wc_barrier);
}

void
memmove_movnt_avx_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx(dest, src, len, flush_clwb_nolog,
			no_barrier_after_ntstores, wc_barrier);
}
