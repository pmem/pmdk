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

static force_inline void
memmove_mov8x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m256i ymm0 = _mm256_loadu_si256((__m256i *)src + 0);
	__m256i ymm1 = _mm256_loadu_si256((__m256i *)src + 1);
	__m256i ymm2 = _mm256_loadu_si256((__m256i *)src + 2);
	__m256i ymm3 = _mm256_loadu_si256((__m256i *)src + 3);
	__m256i ymm4 = _mm256_loadu_si256((__m256i *)src + 4);
	__m256i ymm5 = _mm256_loadu_si256((__m256i *)src + 5);
	__m256i ymm6 = _mm256_loadu_si256((__m256i *)src + 6);
	__m256i ymm7 = _mm256_loadu_si256((__m256i *)src + 7);
	__m256i ymm8 = _mm256_loadu_si256((__m256i *)src + 8);
	__m256i ymm9 = _mm256_loadu_si256((__m256i *)src + 9);
	__m256i ymm10 = _mm256_loadu_si256((__m256i *)src + 10);
	__m256i ymm11 = _mm256_loadu_si256((__m256i *)src + 11);
	__m256i ymm12 = _mm256_loadu_si256((__m256i *)src + 12);
	__m256i ymm13 = _mm256_loadu_si256((__m256i *)src + 13);
	__m256i ymm14 = _mm256_loadu_si256((__m256i *)src + 14);
	__m256i ymm15 = _mm256_loadu_si256((__m256i *)src + 15);

	_mm256_store_si256((__m256i *)dest + 0, ymm0);
	_mm256_store_si256((__m256i *)dest + 1, ymm1);
	_mm256_store_si256((__m256i *)dest + 2, ymm2);
	_mm256_store_si256((__m256i *)dest + 3, ymm3);
	_mm256_store_si256((__m256i *)dest + 4, ymm4);
	_mm256_store_si256((__m256i *)dest + 5, ymm5);
	_mm256_store_si256((__m256i *)dest + 6, ymm6);
	_mm256_store_si256((__m256i *)dest + 7, ymm7);
	_mm256_store_si256((__m256i *)dest + 8, ymm8);
	_mm256_store_si256((__m256i *)dest + 9, ymm9);
	_mm256_store_si256((__m256i *)dest + 10, ymm10);
	_mm256_store_si256((__m256i *)dest + 11, ymm11);
	_mm256_store_si256((__m256i *)dest + 12, ymm12);
	_mm256_store_si256((__m256i *)dest + 13, ymm13);
	_mm256_store_si256((__m256i *)dest + 14, ymm14);
	_mm256_store_si256((__m256i *)dest + 15, ymm15);

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
	__m256i ymm0 = _mm256_loadu_si256((__m256i *)src + 0);
	__m256i ymm1 = _mm256_loadu_si256((__m256i *)src + 1);
	__m256i ymm2 = _mm256_loadu_si256((__m256i *)src + 2);
	__m256i ymm3 = _mm256_loadu_si256((__m256i *)src + 3);
	__m256i ymm4 = _mm256_loadu_si256((__m256i *)src + 4);
	__m256i ymm5 = _mm256_loadu_si256((__m256i *)src + 5);
	__m256i ymm6 = _mm256_loadu_si256((__m256i *)src + 6);
	__m256i ymm7 = _mm256_loadu_si256((__m256i *)src + 7);

	_mm256_store_si256((__m256i *)dest + 0, ymm0);
	_mm256_store_si256((__m256i *)dest + 1, ymm1);
	_mm256_store_si256((__m256i *)dest + 2, ymm2);
	_mm256_store_si256((__m256i *)dest + 3, ymm3);
	_mm256_store_si256((__m256i *)dest + 4, ymm4);
	_mm256_store_si256((__m256i *)dest + 5, ymm5);
	_mm256_store_si256((__m256i *)dest + 6, ymm6);
	_mm256_store_si256((__m256i *)dest + 7, ymm7);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memmove_mov2x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m256i ymm0 = _mm256_loadu_si256((__m256i *)src + 0);
	__m256i ymm1 = _mm256_loadu_si256((__m256i *)src + 1);
	__m256i ymm2 = _mm256_loadu_si256((__m256i *)src + 2);
	__m256i ymm3 = _mm256_loadu_si256((__m256i *)src + 3);

	_mm256_store_si256((__m256i *)dest + 0, ymm0);
	_mm256_store_si256((__m256i *)dest + 1, ymm1);
	_mm256_store_si256((__m256i *)dest + 2, ymm2);
	_mm256_store_si256((__m256i *)dest + 3, ymm3);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memmove_mov1x64b(char *dest, const char *src, flush64b_fn flush64b)
{
	__m256i ymm0 = _mm256_loadu_si256((__m256i *)src + 0);
	__m256i ymm1 = _mm256_loadu_si256((__m256i *)src + 1);

	_mm256_store_si256((__m256i *)dest + 0, ymm0);
	_mm256_store_si256((__m256i *)dest + 1, ymm1);

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

static force_inline void
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
