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
#include "memset_avx.h"

static force_inline void
memset_mov8x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);
	_mm256_store_si256((__m256i *)dest + 2, ymm);
	_mm256_store_si256((__m256i *)dest + 3, ymm);
	_mm256_store_si256((__m256i *)dest + 4, ymm);
	_mm256_store_si256((__m256i *)dest + 5, ymm);
	_mm256_store_si256((__m256i *)dest + 6, ymm);
	_mm256_store_si256((__m256i *)dest + 7, ymm);
	_mm256_store_si256((__m256i *)dest + 8, ymm);
	_mm256_store_si256((__m256i *)dest + 9, ymm);
	_mm256_store_si256((__m256i *)dest + 10, ymm);
	_mm256_store_si256((__m256i *)dest + 11, ymm);
	_mm256_store_si256((__m256i *)dest + 12, ymm);
	_mm256_store_si256((__m256i *)dest + 13, ymm);
	_mm256_store_si256((__m256i *)dest + 14, ymm);
	_mm256_store_si256((__m256i *)dest + 15, ymm);

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
memset_mov4x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);
	_mm256_store_si256((__m256i *)dest + 2, ymm);
	_mm256_store_si256((__m256i *)dest + 3, ymm);
	_mm256_store_si256((__m256i *)dest + 4, ymm);
	_mm256_store_si256((__m256i *)dest + 5, ymm);
	_mm256_store_si256((__m256i *)dest + 6, ymm);
	_mm256_store_si256((__m256i *)dest + 7, ymm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
	flush64b(dest + 2 * 64);
	flush64b(dest + 3 * 64);
}

static force_inline void
memset_mov2x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);
	_mm256_store_si256((__m256i *)dest + 2, ymm);
	_mm256_store_si256((__m256i *)dest + 3, ymm);

	flush64b(dest + 0 * 64);
	flush64b(dest + 1 * 64);
}

static force_inline void
memset_mov1x64b(char *dest, __m256i ymm)
{
	_mm256_store_si256((__m256i *)dest + 0, ymm);
	_mm256_store_si256((__m256i *)dest + 1, ymm);

	flush64b(dest + 0 * 64);
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
		memset_mov8x64b(dest, ymm);
		dest += 8 * 64;
		len -= 8 * 64;
	}

	if (len >= 4 * 64) {
		memset_mov4x64b(dest, ymm);
		dest += 4 * 64;
		len -= 4 * 64;
	}

	if (len >= 2 * 64) {
		memset_mov2x64b(dest, ymm);
		dest += 2 * 64;
		len -= 2 * 64;
	}

	if (len >= 1 * 64) {
		memset_mov1x64b(dest, ymm);

		dest += 1 * 64;
		len -= 1 * 64;
	}

	if (len)
		memset_small_avx(dest, ymm, len);

	avx_zeroupper();
}
