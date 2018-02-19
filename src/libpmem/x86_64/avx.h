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

#ifndef PMEM_AVX_H
#define PMEM_AVX_H

#include <immintrin.h>
#include "util.h"

/*
 * avx_zeroupper -- _mm256_zeroupper wrapper
 *
 * _mm256_zeroupper clears upper parts of avx registers.
 *
 * It's needed for 2 reasons:
 * - it improves performance of non-avx code after avx
 * - it works around problem discovered by Valgrind
 *
 * In optimized builds gcc inserts VZEROUPPER automatically before
 * calling non-avx code (or at the end of the function). But in release
 * builds it doesn't, so if we don't do this by ourselves, then when
 * someone memcpy'ies uninitialized data, Valgrind complains whenever
 * someone reads those registers.
 *
 * One notable example is loader, which tries to detect whether it
 * needs to save whole ymm registers by looking at their current
 * (possibly uninitialized) value.
 *
 * Valgrind complains like that:
 * Conditional jump or move depends on uninitialised value(s)
 *    at 0x4015CC9: _dl_runtime_resolve_avx_slow
 *                                 (in /lib/x86_64-linux-gnu/ld-2.24.so)
 *    by 0x10B531: test_realloc_api (obj_basic_integration.c:185)
 *    by 0x10F1EE: main (obj_basic_integration.c:594)
 *
 * Note: We have to be careful to not read AVX registers after this
 * intrinsic, because of this stupid gcc bug:
 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82735
 */
static force_inline void
avx_zeroupper(void)
{
	_mm256_zeroupper();
}

static force_inline __m128i
m256_get16b(__m256i ymm)
{
	return _mm256_extractf128_si256(ymm, 0);
}

#ifdef _MSC_VER
static force_inline uint64_t
m256_get8b(__m256i ymm)
{
	return (uint64_t)_mm_extract_epi64(m256_get16b(ymm), 0);
}
static force_inline uint32_t
m256_get4b(__m256i ymm)
{
	return (uint32_t)m256_get8b(ymm);
}
static force_inline uint16_t
m256_get2b(__m256i ymm)
{
	return (uint16_t)m256_get8b(ymm);
}
#else
static force_inline uint64_t
m256_get8b(__m256i ymm)
{
	return (uint64_t)_mm256_extract_epi64(ymm, 0);
}
static force_inline uint32_t
m256_get4b(__m256i ymm)
{
	return (uint32_t)_mm256_extract_epi32(ymm, 0);
}
static force_inline uint16_t
m256_get2b(__m256i ymm)
{
	return (uint16_t)_mm256_extract_epi16(ymm, 0);
}
#endif

#endif
