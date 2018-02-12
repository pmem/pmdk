/*
 * Copyright 2017-2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <immintrin.h>
#include <stddef.h>
#include <stdint.h>

#include "pmem.h"
#include "avx.h"
#include "flush.h"
#include "libpmem.h"
#include "memset_avx.h"
#include "memcpy_memset.h"
#include "out.h"
#include "valgrind_internal.h"

static force_inline void
memset_movnt8x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);
	_mm256_stream_si256((__m256i *)dest + 2, ymm);
	_mm256_stream_si256((__m256i *)dest + 3, ymm);
	_mm256_stream_si256((__m256i *)dest + 4, ymm);
	_mm256_stream_si256((__m256i *)dest + 5, ymm);
	_mm256_stream_si256((__m256i *)dest + 6, ymm);
	_mm256_stream_si256((__m256i *)dest + 7, ymm);
	_mm256_stream_si256((__m256i *)dest + 8, ymm);
	_mm256_stream_si256((__m256i *)dest + 9, ymm);
	_mm256_stream_si256((__m256i *)dest + 10, ymm);
	_mm256_stream_si256((__m256i *)dest + 11, ymm);
	_mm256_stream_si256((__m256i *)dest + 12, ymm);
	_mm256_stream_si256((__m256i *)dest + 13, ymm);
	_mm256_stream_si256((__m256i *)dest + 14, ymm);
	_mm256_stream_si256((__m256i *)dest + 15, ymm);

	VALGRIND_DO_FLUSH(dest, 8 * 64);
}

static force_inline void
memset_movnt4x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);
	_mm256_stream_si256((__m256i *)dest + 2, ymm);
	_mm256_stream_si256((__m256i *)dest + 3, ymm);
	_mm256_stream_si256((__m256i *)dest + 4, ymm);
	_mm256_stream_si256((__m256i *)dest + 5, ymm);
	_mm256_stream_si256((__m256i *)dest + 6, ymm);
	_mm256_stream_si256((__m256i *)dest + 7, ymm);

	VALGRIND_DO_FLUSH(dest, 4 * 64);
}

static force_inline void
memset_movnt2x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);
	_mm256_stream_si256((__m256i *)dest + 2, ymm);
	_mm256_stream_si256((__m256i *)dest + 3, ymm);

	VALGRIND_DO_FLUSH(dest, 2 * 64);
}

static force_inline void
memset_movnt1x64b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest + 0, ymm);
	_mm256_stream_si256((__m256i *)dest + 1, ymm);

	VALGRIND_DO_FLUSH(dest, 64);
}

static force_inline void
memset_movnt1x32b(char *dest, __m256i ymm)
{
	_mm256_stream_si256((__m256i *)dest, ymm);

	VALGRIND_DO_FLUSH(dest, 32);
}

static force_inline void
memset_movnt1x16b(char *dest, __m256i ymm)
{
	__m128i xmm0 = m256_get16b(ymm);

	_mm_stream_si128((__m128i *)dest, xmm0);

	VALGRIND_DO_FLUSH(dest - 16, 16);
}

static force_inline void
memset_movnt1x8b(char *dest, __m256i ymm)
{
	uint64_t x = m256_get8b(ymm);

	_mm_stream_si64((long long *)dest, (long long)x);

	VALGRIND_DO_FLUSH(dest, 8);
}

static force_inline void
memset_movnt1x4b(char *dest, __m256i ymm)
{
	uint32_t x = m256_get4b(ymm);

	_mm_stream_si32((int *)dest, (int)x);

	VALGRIND_DO_FLUSH(dest, 4);
}

void
EXPORTED_SYMBOL(char *dest, int c, size_t len)
{
	__m256i ymm = _mm256_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_avx(dest, ymm, cnt);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 8 * 64) {
		memset_movnt8x64b(dest, ymm);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_movnt4x64b(dest, ymm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_movnt2x64b(dest, ymm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_movnt1x64b(dest, ymm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len == 0)
		goto end;

	/* There's no point in using more than 1 nt store for 1 cache line. */
	if (util_is_pow2(len)) {
		if (len == 32)
			memset_movnt1x32b(dest, ymm);
		else if (len == 16)
			memset_movnt1x16b(dest, ymm);
		else if (len == 8)
			memset_movnt1x8b(dest, ymm);
		else if (len == 4)
			memset_movnt1x4b(dest, ymm);
		else
			goto nonnt;

		goto end;
	}

nonnt:
	memset_small_avx(dest, ymm, len);
end:
	avx_zeroupper();

	maybe_barrier();
}
