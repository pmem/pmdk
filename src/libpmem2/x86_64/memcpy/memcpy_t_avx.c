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

static force_inline __m256i
mm256_loadu_si256(const char *src, unsigned idx)
{
	return _mm256_loadu_si256((const __m256i *)src + idx);
}

static force_inline void
mm256_store_si256(char *dest, unsigned idx, __m256i src)
{
	_mm256_store_si256((__m256i *)dest + idx, src);
}

static force_inline void
memmove_mov8x64b(char *dest, const char *src, flush64b_fn flush64b)
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

	mm256_store_si256(dest, 0, ymm0);
	mm256_store_si256(dest, 1, ymm1);
	mm256_store_si256(dest, 2, ymm2);
	mm256_store_si256(dest, 3, ymm3);
	mm256_store_si256(dest, 4, ymm4);
	mm256_store_si256(dest, 5, ymm5);
	mm256_store_si256(dest, 6, ymm6);
	mm256_store_si256(dest, 7, ymm7);
	mm256_store_si256(dest, 8, ymm8);
	mm256_store_si256(dest, 9, ymm9);
	mm256_store_si256(dest, 10, ymm10);
	mm256_store_si256(dest, 11, ymm11);
	mm256_store_si256(dest, 12, ymm12);
	mm256_store_si256(dest, 13, ymm13);
	mm256_store_si256(dest, 14, ymm14);
	mm256_store_si256(dest, 15, ymm15);

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
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);
	__m256i ymm2 = mm256_loadu_si256(src, 2);
	__m256i ymm3 = mm256_loadu_si256(src, 3);
	__m256i ymm4 = mm256_loadu_si256(src, 4);
	__m256i ymm5 = mm256_loadu_si256(src, 5);
	__m256i ymm6 = mm256_loadu_si256(src, 6);
	__m256i ymm7 = mm256_loadu_si256(src, 7);

	mm256_store_si256(dest, 0, ymm0);
	mm256_store_si256(dest, 1, ymm1);
	mm256_store_si256(dest, 2, ymm2);
	mm256_store_si256(dest, 3, ymm3);
	mm256_store_si256(dest, 4, ymm4);
	mm256_store_si256(dest, 5, ymm5);
	mm256_store_si256(dest, 6, ymm6);
	mm256_store_si256(dest, 7, ymm7);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memmove_mov2x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);
	__m256i ymm2 = mm256_loadu_si256(src, 2);
	__m256i ymm3 = mm256_loadu_si256(src, 3);

	mm256_store_si256(dest, 0, ymm0);
	mm256_store_si256(dest, 1, ymm1);
	mm256_store_si256(dest, 2, ymm2);
	mm256_store_si256(dest, 3, ymm3);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memmove_mov1x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m256i ymm0 = mm256_loadu_si256(src, 0);
	__m256i ymm1 = mm256_loadu_si256(src, 1);

	mm256_store_si256(dest, 0, ymm0);
	mm256_store_si256(dest, 1, ymm1);

	flush64b(dest + 0 * 64);
}

static force_inline void
memmove_mov_avx_fw(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
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

	while (len >= 8 * 64) {
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
		memmove_small_avx(dest, src, len, flush);
}

static force_inline void
memmove_mov_avx_bw(char *dest, const char *src, size_t len,
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
		memmove_small_avx(dest, src, cnt, flush);
	}

	while (len >= 8 * 64) {
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
		memmove_small_avx(dest - len, src - len, len, flush);
}

static void
memmove_mov_avx(char *dest, const char *src, size_t len,
		flush_fn flush, flush64b_fn flush64b)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_mov_avx_fw(dest, src, len, flush, flush64b);
	else
		memmove_mov_avx_bw(dest, src, len, flush, flush64b);

	avx_zeroupper();
}

void
memmove_mov_avx_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx(dest, src, len, noflush, noflush64b);
}

void
memmove_mov_avx_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx(dest, src, len, flush_empty_nolog, flush64b_empty);
}

void
memmove_mov_avx_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx(dest, src, len, flush_clflush_nolog, pmem_clflush);
}

void
memmove_mov_avx_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx(dest, src, len, flush_clflushopt_nolog,
			pmem_clflushopt);
}

void
memmove_mov_avx_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_mov_avx(dest, src, len, flush_clwb_nolog, pmem_clwb);
}
