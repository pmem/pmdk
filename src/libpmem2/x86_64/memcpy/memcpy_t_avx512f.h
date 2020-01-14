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

#include "pmem2_arch.h"
#include "avx.h"
#include "flush.h"
#include "memcpy_memset.h"
#include "memcpy_avx512f.h"

static force_inline void
memmove_mov32x64b(char *dest, const char *src)
{
	__m512i zmm0 = _mm512_loadu_si512((__m512i *)src + 0);
	__m512i zmm1 = _mm512_loadu_si512((__m512i *)src + 1);
	__m512i zmm2 = _mm512_loadu_si512((__m512i *)src + 2);
	__m512i zmm3 = _mm512_loadu_si512((__m512i *)src + 3);
	__m512i zmm4 = _mm512_loadu_si512((__m512i *)src + 4);
	__m512i zmm5 = _mm512_loadu_si512((__m512i *)src + 5);
	__m512i zmm6 = _mm512_loadu_si512((__m512i *)src + 6);
	__m512i zmm7 = _mm512_loadu_si512((__m512i *)src + 7);
	__m512i zmm8 = _mm512_loadu_si512((__m512i *)src + 8);
	__m512i zmm9 = _mm512_loadu_si512((__m512i *)src + 9);
	__m512i zmm10 = _mm512_loadu_si512((__m512i *)src + 10);
	__m512i zmm11 = _mm512_loadu_si512((__m512i *)src + 11);
	__m512i zmm12 = _mm512_loadu_si512((__m512i *)src + 12);
	__m512i zmm13 = _mm512_loadu_si512((__m512i *)src + 13);
	__m512i zmm14 = _mm512_loadu_si512((__m512i *)src + 14);
	__m512i zmm15 = _mm512_loadu_si512((__m512i *)src + 15);
	__m512i zmm16 = _mm512_loadu_si512((__m512i *)src + 16);
	__m512i zmm17 = _mm512_loadu_si512((__m512i *)src + 17);
	__m512i zmm18 = _mm512_loadu_si512((__m512i *)src + 18);
	__m512i zmm19 = _mm512_loadu_si512((__m512i *)src + 19);
	__m512i zmm20 = _mm512_loadu_si512((__m512i *)src + 20);
	__m512i zmm21 = _mm512_loadu_si512((__m512i *)src + 21);
	__m512i zmm22 = _mm512_loadu_si512((__m512i *)src + 22);
	__m512i zmm23 = _mm512_loadu_si512((__m512i *)src + 23);
	__m512i zmm24 = _mm512_loadu_si512((__m512i *)src + 24);
	__m512i zmm25 = _mm512_loadu_si512((__m512i *)src + 25);
	__m512i zmm26 = _mm512_loadu_si512((__m512i *)src + 26);
	__m512i zmm27 = _mm512_loadu_si512((__m512i *)src + 27);
	__m512i zmm28 = _mm512_loadu_si512((__m512i *)src + 28);
	__m512i zmm29 = _mm512_loadu_si512((__m512i *)src + 29);
	__m512i zmm30 = _mm512_loadu_si512((__m512i *)src + 30);
	__m512i zmm31 = _mm512_loadu_si512((__m512i *)src + 31);

	_mm512_store_si512((__m512i *)dest + 0, zmm0);
	_mm512_store_si512((__m512i *)dest + 1, zmm1);
	_mm512_store_si512((__m512i *)dest + 2, zmm2);
	_mm512_store_si512((__m512i *)dest + 3, zmm3);
	_mm512_store_si512((__m512i *)dest + 4, zmm4);
	_mm512_store_si512((__m512i *)dest + 5, zmm5);
	_mm512_store_si512((__m512i *)dest + 6, zmm6);
	_mm512_store_si512((__m512i *)dest + 7, zmm7);
	_mm512_store_si512((__m512i *)dest + 8, zmm8);
	_mm512_store_si512((__m512i *)dest + 9, zmm9);
	_mm512_store_si512((__m512i *)dest + 10, zmm10);
	_mm512_store_si512((__m512i *)dest + 11, zmm11);
	_mm512_store_si512((__m512i *)dest + 12, zmm12);
	_mm512_store_si512((__m512i *)dest + 13, zmm13);
	_mm512_store_si512((__m512i *)dest + 14, zmm14);
	_mm512_store_si512((__m512i *)dest + 15, zmm15);
	_mm512_store_si512((__m512i *)dest + 16, zmm16);
	_mm512_store_si512((__m512i *)dest + 17, zmm17);
	_mm512_store_si512((__m512i *)dest + 18, zmm18);
	_mm512_store_si512((__m512i *)dest + 19, zmm19);
	_mm512_store_si512((__m512i *)dest + 20, zmm20);
	_mm512_store_si512((__m512i *)dest + 21, zmm21);
	_mm512_store_si512((__m512i *)dest + 22, zmm22);
	_mm512_store_si512((__m512i *)dest + 23, zmm23);
	_mm512_store_si512((__m512i *)dest + 24, zmm24);
	_mm512_store_si512((__m512i *)dest + 25, zmm25);
	_mm512_store_si512((__m512i *)dest + 26, zmm26);
	_mm512_store_si512((__m512i *)dest + 27, zmm27);
	_mm512_store_si512((__m512i *)dest + 28, zmm28);
	_mm512_store_si512((__m512i *)dest + 29, zmm29);
	_mm512_store_si512((__m512i *)dest + 30, zmm30);
	_mm512_store_si512((__m512i *)dest + 31, zmm31);

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
memmove_mov16x64b(char *dest, const char *src)
{
	__m512i zmm0 = _mm512_loadu_si512((__m512i *)src + 0);
	__m512i zmm1 = _mm512_loadu_si512((__m512i *)src + 1);
	__m512i zmm2 = _mm512_loadu_si512((__m512i *)src + 2);
	__m512i zmm3 = _mm512_loadu_si512((__m512i *)src + 3);
	__m512i zmm4 = _mm512_loadu_si512((__m512i *)src + 4);
	__m512i zmm5 = _mm512_loadu_si512((__m512i *)src + 5);
	__m512i zmm6 = _mm512_loadu_si512((__m512i *)src + 6);
	__m512i zmm7 = _mm512_loadu_si512((__m512i *)src + 7);
	__m512i zmm8 = _mm512_loadu_si512((__m512i *)src + 8);
	__m512i zmm9 = _mm512_loadu_si512((__m512i *)src + 9);
	__m512i zmm10 = _mm512_loadu_si512((__m512i *)src + 10);
	__m512i zmm11 = _mm512_loadu_si512((__m512i *)src + 11);
	__m512i zmm12 = _mm512_loadu_si512((__m512i *)src + 12);
	__m512i zmm13 = _mm512_loadu_si512((__m512i *)src + 13);
	__m512i zmm14 = _mm512_loadu_si512((__m512i *)src + 14);
	__m512i zmm15 = _mm512_loadu_si512((__m512i *)src + 15);

	_mm512_store_si512((__m512i *)dest + 0, zmm0);
	_mm512_store_si512((__m512i *)dest + 1, zmm1);
	_mm512_store_si512((__m512i *)dest + 2, zmm2);
	_mm512_store_si512((__m512i *)dest + 3, zmm3);
	_mm512_store_si512((__m512i *)dest + 4, zmm4);
	_mm512_store_si512((__m512i *)dest + 5, zmm5);
	_mm512_store_si512((__m512i *)dest + 6, zmm6);
	_mm512_store_si512((__m512i *)dest + 7, zmm7);
	_mm512_store_si512((__m512i *)dest + 8, zmm8);
	_mm512_store_si512((__m512i *)dest + 9, zmm9);
	_mm512_store_si512((__m512i *)dest + 10, zmm10);
	_mm512_store_si512((__m512i *)dest + 11, zmm11);
	_mm512_store_si512((__m512i *)dest + 12, zmm12);
	_mm512_store_si512((__m512i *)dest + 13, zmm13);
	_mm512_store_si512((__m512i *)dest + 14, zmm14);
	_mm512_store_si512((__m512i *)dest + 15, zmm15);

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
memmove_mov8x64b(char *dest, const char *src)
{
	__m512i zmm0 = _mm512_loadu_si512((__m512i *)src + 0);
	__m512i zmm1 = _mm512_loadu_si512((__m512i *)src + 1);
	__m512i zmm2 = _mm512_loadu_si512((__m512i *)src + 2);
	__m512i zmm3 = _mm512_loadu_si512((__m512i *)src + 3);
	__m512i zmm4 = _mm512_loadu_si512((__m512i *)src + 4);
	__m512i zmm5 = _mm512_loadu_si512((__m512i *)src + 5);
	__m512i zmm6 = _mm512_loadu_si512((__m512i *)src + 6);
	__m512i zmm7 = _mm512_loadu_si512((__m512i *)src + 7);

	_mm512_store_si512((__m512i *)dest + 0, zmm0);
	_mm512_store_si512((__m512i *)dest + 1, zmm1);
	_mm512_store_si512((__m512i *)dest + 2, zmm2);
	_mm512_store_si512((__m512i *)dest + 3, zmm3);
	_mm512_store_si512((__m512i *)dest + 4, zmm4);
	_mm512_store_si512((__m512i *)dest + 5, zmm5);
	_mm512_store_si512((__m512i *)dest + 6, zmm6);
	_mm512_store_si512((__m512i *)dest + 7, zmm7);

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
memmove_mov4x64b(char *dest, const char *src)
{
	__m512i zmm0 = _mm512_loadu_si512((__m512i *)src + 0);
	__m512i zmm1 = _mm512_loadu_si512((__m512i *)src + 1);
	__m512i zmm2 = _mm512_loadu_si512((__m512i *)src + 2);
	__m512i zmm3 = _mm512_loadu_si512((__m512i *)src + 3);

	_mm512_store_si512((__m512i *)dest + 0, zmm0);
	_mm512_store_si512((__m512i *)dest + 1, zmm1);
	_mm512_store_si512((__m512i *)dest + 2, zmm2);
	_mm512_store_si512((__m512i *)dest + 3, zmm3);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memmove_mov2x64b(char *dest, const char *src)
{
	__m512i zmm0 = _mm512_loadu_si512((__m512i *)src + 0);
	__m512i zmm1 = _mm512_loadu_si512((__m512i *)src + 1);

	_mm512_store_si512((__m512i *)dest + 0, zmm0);
	_mm512_store_si512((__m512i *)dest + 1, zmm1);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memmove_mov1x64b(char *dest, const char *src)
{
	__m512i zmm0 = _mm512_loadu_si512((__m512i *)src + 0);

	_mm512_store_si512((__m512i *)dest + 0, zmm0);

	flush64b(dest + 0 * 64);
}

static force_inline void
memmove_mov_avx512f_fw(char *dest, const char *src, size_t len)
{
	size_t cnt = (uint64_t)dest & 63;
	if (cnt > 0) {
		cnt = 64 - cnt;

		if (cnt > len)
			cnt = len;

		memmove_small_avx512f(dest, src, cnt);

		dest += cnt;
		src += cnt;
		len -= cnt;
	}

	while (len >= 32 * 64) {
		memmove_mov32x64b(dest, src);
		dest += 32 * 64;
		src += 32 * 64;
		len -= 32 * 64;
	}

	if (len >= 16 * 64) {
		memmove_mov16x64b(dest, src);
		dest += 16 * 64;
		src += 16 * 64;
		len -= 16 * 64;
	}

	if (len >= 8 * 64) {
		memmove_mov8x64b(dest, src);
		dest += 8 * 64;
		src += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memmove_mov4x64b(dest, src);
		dest += 4 * 64;
		src += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memmove_mov2x64b(dest, src);
		dest += 2 * 64;
		src += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memmove_mov1x64b(dest, src);

		dest += 1 * 64;
		src += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memmove_small_avx512f(dest, src, len);
}

static force_inline void
memmove_mov_avx512f_bw(char *dest, const char *src, size_t len)
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

		memmove_small_avx512f(dest, src, cnt);
	}

	while (len >= 32 * 64) {
		dest -= 32 * 64;
		src -= 32 * 64;
		len -= 32 * 64;
		memmove_mov32x64b(dest, src);
	}

	if (len >= 16 * 64) {
		dest -= 16 * 64;
		src -= 16 * 64;
		len -= 16 * 64;
		memmove_mov16x64b(dest, src);
	}

	if (len >= 8 * 64) {
		dest -= 8 * 64;
		src -= 8 * 64;
		len -= 8 * 64;
		memmove_mov8x64b(dest, src);
	}

	if (len >= 4 * 64) {
		dest -= 4 * 64;
		src -= 4 * 64;
		len -= 4 * 64;
		memmove_mov4x64b(dest, src);
	}

	if (len >= 2 * 64) {
		dest -= 2 * 64;
		src -= 2 * 64;
		len -= 2 * 64;
		memmove_mov2x64b(dest, src);
	}

	if (len >= 1 * 64) {
		dest -= 1 * 64;
		src -= 1 * 64;
		len -= 1 * 64;
		memmove_mov1x64b(dest, src);
	}

	if (len)
		memmove_small_avx512f(dest - len, src - len, len);
}

void
EXPORTED_SYMBOL(char *dest, const char *src, size_t len)
{
	if ((uintptr_t)dest - (uintptr_t)src >= len)
		memmove_mov_avx512f_fw(dest, src, len);
	else
		memmove_mov_avx512f_bw(dest, src, len);

	avx_zeroupper();
}
