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
#include "valgrind_internal.h"

static force_inline __m512i
mm512_loadu_si512(const char *src, unsigned idx)
{
	return _mm512_loadu_si512((const __m512i *)src + idx);
}

static force_inline void
mm512_stream_si512(char *dest, unsigned idx, __m512i src)
{
	_mm512_stream_si512((__m512i *)dest + idx, src);
}

static force_inline void
memmove_movnt32x64b(char *dest, const char *src)
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

	mm512_stream_si512(dest, 0, zmm0);
	mm512_stream_si512(dest, 1, zmm1);
	mm512_stream_si512(dest, 2, zmm2);
	mm512_stream_si512(dest, 3, zmm3);
	mm512_stream_si512(dest, 4, zmm4);
	mm512_stream_si512(dest, 5, zmm5);
	mm512_stream_si512(dest, 6, zmm6);
	mm512_stream_si512(dest, 7, zmm7);
	mm512_stream_si512(dest, 8, zmm8);
	mm512_stream_si512(dest, 9, zmm9);
	mm512_stream_si512(dest, 10, zmm10);
	mm512_stream_si512(dest, 11, zmm11);
	mm512_stream_si512(dest, 12, zmm12);
	mm512_stream_si512(dest, 13, zmm13);
	mm512_stream_si512(dest, 14, zmm14);
	mm512_stream_si512(dest, 15, zmm15);
	mm512_stream_si512(dest, 16, zmm16);
	mm512_stream_si512(dest, 17, zmm17);
	mm512_stream_si512(dest, 18, zmm18);
	mm512_stream_si512(dest, 19, zmm19);
	mm512_stream_si512(dest, 20, zmm20);
	mm512_stream_si512(dest, 21, zmm21);
	mm512_stream_si512(dest, 22, zmm22);
	mm512_stream_si512(dest, 23, zmm23);
	mm512_stream_si512(dest, 24, zmm24);
	mm512_stream_si512(dest, 25, zmm25);
	mm512_stream_si512(dest, 26, zmm26);
	mm512_stream_si512(dest, 27, zmm27);
	mm512_stream_si512(dest, 28, zmm28);
	mm512_stream_si512(dest, 29, zmm29);
	mm512_stream_si512(dest, 30, zmm30);
	mm512_stream_si512(dest, 31, zmm31);
}

static force_inline void
memmove_movnt16x64b(char *dest, const char *src)
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

	mm512_stream_si512(dest, 0, zmm0);
	mm512_stream_si512(dest, 1, zmm1);
	mm512_stream_si512(dest, 2, zmm2);
	mm512_stream_si512(dest, 3, zmm3);
	mm512_stream_si512(dest, 4, zmm4);
	mm512_stream_si512(dest, 5, zmm5);
	mm512_stream_si512(dest, 6, zmm6);
	mm512_stream_si512(dest, 7, zmm7);
	mm512_stream_si512(dest, 8, zmm8);
	mm512_stream_si512(dest, 9, zmm9);
	mm512_stream_si512(dest, 10, zmm10);
	mm512_stream_si512(dest, 11, zmm11);
	mm512_stream_si512(dest, 12, zmm12);
	mm512_stream_si512(dest, 13, zmm13);
	mm512_stream_si512(dest, 14, zmm14);
	mm512_stream_si512(dest, 15, zmm15);
}

static force_inline void
memmove_movnt8x64b(char *dest, const char *src)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);
	__m512i zmm2 = mm512_loadu_si512(src, 2);
	__m512i zmm3 = mm512_loadu_si512(src, 3);
	__m512i zmm4 = mm512_loadu_si512(src, 4);
	__m512i zmm5 = mm512_loadu_si512(src, 5);
	__m512i zmm6 = mm512_loadu_si512(src, 6);
	__m512i zmm7 = mm512_loadu_si512(src, 7);

	mm512_stream_si512(dest, 0, zmm0);
	mm512_stream_si512(dest, 1, zmm1);
	mm512_stream_si512(dest, 2, zmm2);
	mm512_stream_si512(dest, 3, zmm3);
	mm512_stream_si512(dest, 4, zmm4);
	mm512_stream_si512(dest, 5, zmm5);
	mm512_stream_si512(dest, 6, zmm6);
	mm512_stream_si512(dest, 7, zmm7);
}

static force_inline void
memmove_movnt4x64b(char *dest, const char *src)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);
	__m512i zmm2 = mm512_loadu_si512(src, 2);
	__m512i zmm3 = mm512_loadu_si512(src, 3);

	mm512_stream_si512(dest, 0, zmm0);
	mm512_stream_si512(dest, 1, zmm1);
	mm512_stream_si512(dest, 2, zmm2);
	mm512_stream_si512(dest, 3, zmm3);
}

static force_inline void
memmove_movnt2x64b(char *dest, const char *src)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);
	__m512i zmm1 = mm512_loadu_si512(src, 1);

	mm512_stream_si512(dest, 0, zmm0);
	mm512_stream_si512(dest, 1, zmm1);
}

static force_inline void
memmove_movnt1x64b(char *dest, const char *src)
{
	__m512i zmm0 = mm512_loadu_si512(src, 0);

	mm512_stream_si512(dest, 0, zmm0);
}

static force_inline void
memmove_movnt1x32b(char *dest, const char *src)
{
	__m256i zmm0 = _mm256_loadu_si256((__m256i *)src);

	_mm256_stream_si256((__m256i *)dest, zmm0);
}

static force_inline void
memmove_movnt1x16b(char *dest, const char *src)
{
	__m128i ymm0 = _mm_loadu_si128((__m128i *)src);

	_mm_stream_si128((__m128i *)dest, ymm0);
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
memmove_movnt_avx512f_fw(char *dest, const char *src, size_t len,
		flush_fn flush)
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
		memmove_movnt32x64b(dest, src);
		dest += 32 * 64;
		src += 32 * 64;
		len -= 32 * 64;
	}

	if (len >= 16 * 64) {
		memmove_movnt16x64b(dest, src);
		dest += 16 * 64;
		src += 16 * 64;
		len -= 16 * 64;
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
	memmove_small_avx512f(dest, src, len, flush);
end:
	avx_zeroupper();
}

static force_inline void
memmove_movnt_avx512f_bw(char *dest, const char *src, size_t len,
		flush_fn flush)
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
		memmove_movnt32x64b(dest, src);
	}

	if (len >= 16 * 64) {
		dest -= 16 * 64;
		src -= 16 * 64;
		len -= 16 * 64;
		memmove_movnt16x64b(dest, src);
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

	memmove_small_avx512f(dest, src, len, flush);
end:
	avx_zeroupper();
}

static force_inline void
memmove_movnt_avx512f(char *dest, const char *src, size_t len, flush_fn flush,
		barrier_fn barrier)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_movnt_avx512f_fw(dest, src, len, flush);
	else
		memmove_movnt_avx512f_bw(dest, src, len, flush);

	barrier();

	VALGRIND_DO_FLUSH(dest, len);
}

void
memmove_movnt_avx512f_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx512f(dest, src, len, noflush, barrier_after_ntstores);
}

void
memmove_movnt_avx512f_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx512f(dest, src, len, flush_empty_nolog,
			barrier_after_ntstores);
}

void
memmove_movnt_avx512f_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx512f(dest, src, len, flush_clflush_nolog,
			barrier_after_ntstores);
}

void
memmove_movnt_avx512f_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx512f(dest, src, len, flush_clflushopt_nolog,
			no_barrier_after_ntstores);
}

void
memmove_movnt_avx512f_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_avx512f(dest, src, len, flush_clwb_nolog,
			no_barrier_after_ntstores);
}
