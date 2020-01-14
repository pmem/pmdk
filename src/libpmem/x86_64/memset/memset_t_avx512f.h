/*
 * Copyright 2017-2020, Intel Corporation
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
#include "memcpy_memset.h"
#include "memset_avx512f.h"

static force_inline void
memset_mov32x64b(char *dest, __m512i zmm)
{
	_mm512_store_si512((__m512i *)dest + 0, zmm);
	_mm512_store_si512((__m512i *)dest + 1, zmm);
	_mm512_store_si512((__m512i *)dest + 2, zmm);
	_mm512_store_si512((__m512i *)dest + 3, zmm);
	_mm512_store_si512((__m512i *)dest + 4, zmm);
	_mm512_store_si512((__m512i *)dest + 5, zmm);
	_mm512_store_si512((__m512i *)dest + 6, zmm);
	_mm512_store_si512((__m512i *)dest + 7, zmm);
	_mm512_store_si512((__m512i *)dest + 8, zmm);
	_mm512_store_si512((__m512i *)dest + 9, zmm);
	_mm512_store_si512((__m512i *)dest + 10, zmm);
	_mm512_store_si512((__m512i *)dest + 11, zmm);
	_mm512_store_si512((__m512i *)dest + 12, zmm);
	_mm512_store_si512((__m512i *)dest + 13, zmm);
	_mm512_store_si512((__m512i *)dest + 14, zmm);
	_mm512_store_si512((__m512i *)dest + 15, zmm);
	_mm512_store_si512((__m512i *)dest + 16, zmm);
	_mm512_store_si512((__m512i *)dest + 17, zmm);
	_mm512_store_si512((__m512i *)dest + 18, zmm);
	_mm512_store_si512((__m512i *)dest + 19, zmm);
	_mm512_store_si512((__m512i *)dest + 20, zmm);
	_mm512_store_si512((__m512i *)dest + 21, zmm);
	_mm512_store_si512((__m512i *)dest + 22, zmm);
	_mm512_store_si512((__m512i *)dest + 23, zmm);
	_mm512_store_si512((__m512i *)dest + 24, zmm);
	_mm512_store_si512((__m512i *)dest + 25, zmm);
	_mm512_store_si512((__m512i *)dest + 26, zmm);
	_mm512_store_si512((__m512i *)dest + 27, zmm);
	_mm512_store_si512((__m512i *)dest + 28, zmm);
	_mm512_store_si512((__m512i *)dest + 29, zmm);
	_mm512_store_si512((__m512i *)dest + 30, zmm);
	_mm512_store_si512((__m512i *)dest + 31, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
	flush64b(dest + 4 * 64);
	flush64b(dest + 5 * 64);
	flush64b(dest + 6 * 64);
	flush64b(dest + 7 * 64);
	flush64b(dest + 8 * 64);
	flush64b(dest + 9 * 64);
	flush64b(dest + 10 * 64);
	flush64b(dest + 11 * 64);
	flush64b(dest + 12 * 64);
	flush64b(dest + 13 * 64);
	flush64b(dest + 14 * 64);
	flush64b(dest + 15 * 64);
	flush64b(dest + 16 * 64);
	flush64b(dest + 17 * 64);
	flush64b(dest + 18 * 64);
	flush64b(dest + 19 * 64);
	flush64b(dest + 20 * 64);
	flush64b(dest + 21 * 64);
	flush64b(dest + 22 * 64);
	flush64b(dest + 23 * 64);
	flush64b(dest + 24 * 64);
	flush64b(dest + 25 * 64);
	flush64b(dest + 26 * 64);
	flush64b(dest + 27 * 64);
	flush64b(dest + 28 * 64);
	flush64b(dest + 29 * 64);
	flush64b(dest + 30 * 64);
	flush64b(dest + 31 * 64);
}

static force_inline void
memset_mov16x64b(char *dest, __m512i zmm)
{
	_mm512_store_si512((__m512i *)dest + 0, zmm);
	_mm512_store_si512((__m512i *)dest + 1, zmm);
	_mm512_store_si512((__m512i *)dest + 2, zmm);
	_mm512_store_si512((__m512i *)dest + 3, zmm);
	_mm512_store_si512((__m512i *)dest + 4, zmm);
	_mm512_store_si512((__m512i *)dest + 5, zmm);
	_mm512_store_si512((__m512i *)dest + 6, zmm);
	_mm512_store_si512((__m512i *)dest + 7, zmm);
	_mm512_store_si512((__m512i *)dest + 8, zmm);
	_mm512_store_si512((__m512i *)dest + 9, zmm);
	_mm512_store_si512((__m512i *)dest + 10, zmm);
	_mm512_store_si512((__m512i *)dest + 11, zmm);
	_mm512_store_si512((__m512i *)dest + 12, zmm);
	_mm512_store_si512((__m512i *)dest + 13, zmm);
	_mm512_store_si512((__m512i *)dest + 14, zmm);
	_mm512_store_si512((__m512i *)dest + 15, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
	flush64b(dest + 4 * 64);
	flush64b(dest + 5 * 64);
	flush64b(dest + 6 * 64);
	flush64b(dest + 7 * 64);
	flush64b(dest + 8 * 64);
	flush64b(dest + 9 * 64);
	flush64b(dest + 10 * 64);
	flush64b(dest + 11 * 64);
	flush64b(dest + 12 * 64);
	flush64b(dest + 13 * 64);
	flush64b(dest + 14 * 64);
	flush64b(dest + 15 * 64);
}

static force_inline void
memset_mov8x64b(char *dest, __m512i zmm)
{
	_mm512_store_si512((__m512i *)dest + 0, zmm);
	_mm512_store_si512((__m512i *)dest + 1, zmm);
	_mm512_store_si512((__m512i *)dest + 2, zmm);
	_mm512_store_si512((__m512i *)dest + 3, zmm);
	_mm512_store_si512((__m512i *)dest + 4, zmm);
	_mm512_store_si512((__m512i *)dest + 5, zmm);
	_mm512_store_si512((__m512i *)dest + 6, zmm);
	_mm512_store_si512((__m512i *)dest + 7, zmm);

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
memset_mov4x64b(char *dest, __m512i zmm)
{
	_mm512_store_si512((__m512i *)dest + 0, zmm);
	_mm512_store_si512((__m512i *)dest + 1, zmm);
	_mm512_store_si512((__m512i *)dest + 2, zmm);
	_mm512_store_si512((__m512i *)dest + 3, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memset_mov2x64b(char *dest, __m512i zmm)
{
	_mm512_store_si512((__m512i *)dest + 0, zmm);
	_mm512_store_si512((__m512i *)dest + 1, zmm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memset_mov1x64b(char *dest, __m512i zmm)
{
	_mm512_store_si512((__m512i *)dest + 0, zmm);

	flush64b(dest + 0 * 64);
}

void
EXPORTED_SYMBOL(char *dest, int c, size_t len)
{
	__m512i zmm = _mm512_set1_epi8((char)c);
	/* See comment in memset_movnt_avx512f */
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
		memset_mov32x64b(dest, zmm);
		dest += 32 * 64;
		len -= 32 * 64;
	}

	if (len >= 16 * 64) {
		memset_mov16x64b(dest, zmm);
		dest += 16 * 64;
		len -= 16 * 64;
	}

	if (len >= 8 * 64) {
		memset_mov8x64b(dest, zmm);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_mov4x64b(dest, zmm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_mov2x64b(dest, zmm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_mov1x64b(dest, zmm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memset_small_avx512f(dest, ymm, len);

	avx_zeroupper();
}
