// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_movdir64b.h"
#include "valgrind_internal.h"

static force_inline void
movdir64b(char *dest, const char *src)
{
	_movdir64b(dest, src);
	compiler_barrier();
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
memmove_movnt_movdir64b_fw(char *dest, const char *src, size_t len,
		flush_fn flush)
{
	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memmove_small_movdir64b(dest, src, cnt, flush);

		dest += cnt;
		src += cnt;
		len -= cnt;
	}

	while (len >= 64) {
		movdir64b(dest, src);
		dest += 64;
		src += 64;
		len -= 64;
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
	memmove_small_movdir64b(dest, src, len, flush);
end:
	avx_zeroupper(); /* AVX instructions are used for smaller moves */
}

static force_inline void
memmove_movnt_movdir64b_bw(char *dest, const char *src, size_t len,
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

		memmove_small_movdir64b(dest, src, cnt, flush);
	}

	while (len >= 64) {
		dest -= 64;
		src -= 64;
		len -= 64;

		movdir64b(dest, src);
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

	memmove_small_movdir64b(dest, src, len, flush);
end:
	avx_zeroupper(); /* AVX instructions are used for smaller moves */
}

static force_inline void
memmove_movnt_movdir64b(char *dest, const char *src, size_t len, flush_fn flush,
		barrier_fn barrier)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_movnt_movdir64b_fw(dest, src, len, flush);
	else
		memmove_movnt_movdir64b_bw(dest, src, len, flush);

	barrier();

	VALGRIND_DO_FLUSH(dest, len);
}

void
memmove_movnt_movdir64b_noflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_movdir64b(dest, src, len, noflush,
			barrier_after_ntstores);
}

void
memmove_movnt_movdir64b_empty(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_movdir64b(dest, src, len, flush_empty_nolog,
			barrier_after_ntstores);
}

void
memmove_movnt_movdir64b_clflush(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_movdir64b(dest, src, len, flush_clflush_nolog,
			barrier_after_ntstores);
}

void
memmove_movnt_movdir64b_clflushopt(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_movdir64b(dest, src, len, flush_clflushopt_nolog,
			no_barrier_after_ntstores);
}

void
memmove_movnt_movdir64b_clwb(char *dest, const char *src, size_t len)
{
	LOG(15, "dest %p src %p len %zu", dest, src, len);

	memmove_movnt_movdir64b(dest, src, len, flush_clwb_nolog,
			no_barrier_after_ntstores);
}
