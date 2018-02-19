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
#include "memcpy_memset.h"
#include "memset_avx512f.h"
#include "out.h"
#include "util.h"
#include "valgrind_internal.h"

static force_inline void
memset_movnt32x64b(char *dest, __m512i zmm)
{
	_mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);
	_mm512_stream_si512((__m512i *)dest + 4, zmm);
	_mm512_stream_si512((__m512i *)dest + 5, zmm);
	_mm512_stream_si512((__m512i *)dest + 6, zmm);
	_mm512_stream_si512((__m512i *)dest + 7, zmm);
	_mm512_stream_si512((__m512i *)dest + 8, zmm);
	_mm512_stream_si512((__m512i *)dest + 9, zmm);
	_mm512_stream_si512((__m512i *)dest + 10, zmm);
	_mm512_stream_si512((__m512i *)dest + 11, zmm);
	_mm512_stream_si512((__m512i *)dest + 12, zmm);
	_mm512_stream_si512((__m512i *)dest + 13, zmm);
	_mm512_stream_si512((__m512i *)dest + 14, zmm);
	_mm512_stream_si512((__m512i *)dest + 15, zmm);
	_mm512_stream_si512((__m512i *)dest + 16, zmm);
	_mm512_stream_si512((__m512i *)dest + 17, zmm);
	_mm512_stream_si512((__m512i *)dest + 18, zmm);
	_mm512_stream_si512((__m512i *)dest + 19, zmm);
	_mm512_stream_si512((__m512i *)dest + 20, zmm);
	_mm512_stream_si512((__m512i *)dest + 21, zmm);
	_mm512_stream_si512((__m512i *)dest + 22, zmm);
	_mm512_stream_si512((__m512i *)dest + 23, zmm);
	_mm512_stream_si512((__m512i *)dest + 24, zmm);
	_mm512_stream_si512((__m512i *)dest + 25, zmm);
	_mm512_stream_si512((__m512i *)dest + 26, zmm);
	_mm512_stream_si512((__m512i *)dest + 27, zmm);
	_mm512_stream_si512((__m512i *)dest + 28, zmm);
	_mm512_stream_si512((__m512i *)dest + 29, zmm);
	_mm512_stream_si512((__m512i *)dest + 30, zmm);
	_mm512_stream_si512((__m512i *)dest + 31, zmm);

	VALGRIND_DO_FLUSH(dest, 32 * 64);
}

static force_inline void
memset_movnt16x64b(char *dest, __m512i zmm)
{
	_mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);
	_mm512_stream_si512((__m512i *)dest + 4, zmm);
	_mm512_stream_si512((__m512i *)dest + 5, zmm);
	_mm512_stream_si512((__m512i *)dest + 6, zmm);
	_mm512_stream_si512((__m512i *)dest + 7, zmm);
	_mm512_stream_si512((__m512i *)dest + 8, zmm);
	_mm512_stream_si512((__m512i *)dest + 9, zmm);
	_mm512_stream_si512((__m512i *)dest + 10, zmm);
	_mm512_stream_si512((__m512i *)dest + 11, zmm);
	_mm512_stream_si512((__m512i *)dest + 12, zmm);
	_mm512_stream_si512((__m512i *)dest + 13, zmm);
	_mm512_stream_si512((__m512i *)dest + 14, zmm);
	_mm512_stream_si512((__m512i *)dest + 15, zmm);

	VALGRIND_DO_FLUSH(dest, 16 * 64);
}

static force_inline void
memset_movnt8x64b(char *dest, __m512i zmm)
{
	_mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);
	_mm512_stream_si512((__m512i *)dest + 4, zmm);
	_mm512_stream_si512((__m512i *)dest + 5, zmm);
	_mm512_stream_si512((__m512i *)dest + 6, zmm);
	_mm512_stream_si512((__m512i *)dest + 7, zmm);

	VALGRIND_DO_FLUSH(dest, 8 * 64);
}

static force_inline void
memset_movnt4x64b(char *dest, __m512i zmm)
{
	_mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);
	_mm512_stream_si512((__m512i *)dest + 2, zmm);
	_mm512_stream_si512((__m512i *)dest + 3, zmm);

	VALGRIND_DO_FLUSH(dest, 4 * 64);
}

static force_inline void
memset_movnt2x64b(char *dest, __m512i zmm)
{
	_mm512_stream_si512((__m512i *)dest + 0, zmm);
	_mm512_stream_si512((__m512i *)dest + 1, zmm);

	VALGRIND_DO_FLUSH(dest, 2 * 64);
}

static force_inline void
memset_movnt1x64b(char *dest, __m512i zmm)
{
	_mm512_stream_si512((__m512i *)dest + 0, zmm);

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
	__m128i xmm = _mm256_extracti128_si256(ymm, 0);

	_mm_stream_si128((__m128i *)dest, xmm);

	VALGRIND_DO_FLUSH(dest, 16);
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
	__m512i zmm = _mm512_set1_epi8((char)c);
	/*
	 * Can't use _mm512_extracti64x4_epi64, because some versions of gcc
	 * crash. https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82887
	 */
	__m256i ymm = _mm256_set1_epi8((char)c);

	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memset_small_avx512f(dest, ymm, cnt);

		dest += cnt;
		len -= cnt;
	}

	while (len >= 32 * 64) {
		memset_movnt32x64b(dest, zmm);
		dest += 32 * 64;
		len -= 32 * 64;
	}

	if (len >= 16 * 64) {
		memset_movnt16x64b(dest, zmm);
		dest += 16 * 64;
		len -= 16 * 64;
	}

	if (len >= 8 * 64) {
		memset_movnt8x64b(dest, zmm);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_movnt4x64b(dest, zmm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_movnt2x64b(dest, zmm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_movnt1x64b(dest, zmm);

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
	memset_small_avx512f(dest, ymm, len);
end:
	avx_zeroupper();

	maybe_barrier();
}
