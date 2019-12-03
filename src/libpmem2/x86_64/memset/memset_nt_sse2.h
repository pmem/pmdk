/*
 * Copyright 2017-2019, Intel Corporation
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

#include "pmem2_arch.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memset_sse2.h"
#include "out.h"
#include "valgrind_internal.h"

static force_inline void
memset_movnt4x64b(char *dest, __m128i xmm)
{
	_mm_stream_si128((__m128i *)dest + 0, xmm);
	_mm_stream_si128((__m128i *)dest + 1, xmm);
	_mm_stream_si128((__m128i *)dest + 2, xmm);
	_mm_stream_si128((__m128i *)dest + 3, xmm);
	_mm_stream_si128((__m128i *)dest + 4, xmm);
	_mm_stream_si128((__m128i *)dest + 5, xmm);
	_mm_stream_si128((__m128i *)dest + 6, xmm);
	_mm_stream_si128((__m128i *)dest + 7, xmm);
	_mm_stream_si128((__m128i *)dest + 8, xmm);
	_mm_stream_si128((__m128i *)dest + 9, xmm);
	_mm_stream_si128((__m128i *)dest + 10, xmm);
	_mm_stream_si128((__m128i *)dest + 11, xmm);
	_mm_stream_si128((__m128i *)dest + 12, xmm);
	_mm_stream_si128((__m128i *)dest + 13, xmm);
	_mm_stream_si128((__m128i *)dest + 14, xmm);
	_mm_stream_si128((__m128i *)dest + 15, xmm);

	VALGRIND_DO_FLUSH(dest, 4 * 64);
}

static force_inline void
memset_movnt2x64b(char *dest, __m128i xmm)
{
	_mm_stream_si128((__m128i *)dest + 0, xmm);
	_mm_stream_si128((__m128i *)dest + 1, xmm);
	_mm_stream_si128((__m128i *)dest + 2, xmm);
	_mm_stream_si128((__m128i *)dest + 3, xmm);
	_mm_stream_si128((__m128i *)dest + 4, xmm);
	_mm_stream_si128((__m128i *)dest + 5, xmm);
	_mm_stream_si128((__m128i *)dest + 6, xmm);
	_mm_stream_si128((__m128i *)dest + 7, xmm);

	VALGRIND_DO_FLUSH(dest, 2 * 64);
}

static force_inline void
memset_movnt1x64b(char *dest, __m128i xmm)
{
	_mm_stream_si128((__m128i *)dest + 0, xmm);
	_mm_stream_si128((__m128i *)dest + 1, xmm);
	_mm_stream_si128((__m128i *)dest + 2, xmm);
	_mm_stream_si128((__m128i *)dest + 3, xmm);

	VALGRIND_DO_FLUSH(dest, 64);
}

static force_inline void
memset_movnt1x32b(char *dest, __m128i xmm)
{
	_mm_stream_si128((__m128i *)dest + 0, xmm);
	_mm_stream_si128((__m128i *)dest + 1, xmm);

	VALGRIND_DO_FLUSH(dest, 32);
}

static force_inline void
memset_movnt1x16b(char *dest, __m128i xmm)
{
	_mm_stream_si128((__m128i *)dest, xmm);

	VALGRIND_DO_FLUSH(dest, 16);
}

static force_inline void
memset_movnt1x8b(char *dest, __m128i xmm)
{
	uint64_t x = (uint64_t)_mm_cvtsi128_si64(xmm);

	_mm_stream_si64((long long *)dest, (long long)x);

	VALGRIND_DO_FLUSH(dest, 8);
}

static force_inline void
memset_movnt1x4b(char *dest, __m128i xmm)
{
	uint32_t x = (uint32_t)_mm_cvtsi128_si32(xmm);

	_mm_stream_si32((int *)dest, (int)x);

	VALGRIND_DO_FLUSH(dest, 4);
}

void
EXPORTED_SYMBOL(char *dest, int c, size_t len)
{
	__m128i xmm = _mm_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_sse2(dest, xmm, cnt);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 4 * 64) {
		memset_movnt4x64b(dest, xmm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_movnt2x64b(dest, xmm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_movnt1x64b(dest, xmm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len == 0)
		goto end;

	/* There's no point in using more than 1 nt store for 1 cache line. */
	if (util_is_pow2(len)) {
		if (len == 32)
			memset_movnt1x32b(dest, xmm);
		else if (len == 16)
			memset_movnt1x16b(dest, xmm);
		else if (len == 8)
			memset_movnt1x8b(dest, xmm);
		else if (len == 4)
			memset_movnt1x4b(dest, xmm);
		else
			goto nonnt;

		goto end;
	}

nonnt:
	memset_small_sse2(dest, xmm, len);
end:
	maybe_barrier();
}
