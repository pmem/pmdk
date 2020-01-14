/*
 * Copyright 2014-2020, Intel Corporation
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

#ifndef MEMCPY_MEMSET_H
#define MEMCPY_MEMSET_H

#include <stddef.h>
#include <xmmintrin.h>
#include "pmem2_arch.h"

static inline void
barrier_after_ntstores(void)
{
	/*
	 * In this configuration pmem_drain does not contain sfence, so we have
	 * to serialize non-temporal store instructions.
	 */
	_mm_sfence();
}

static inline void
no_barrier_after_ntstores(void)
{
	/*
	 * In this configuration pmem_drain contains sfence, so we don't have
	 * to serialize non-temporal store instructions
	 */
}

static inline void
noflush(const void *addr, size_t len)
{
	/* NOP, not even pmemcheck annotation */
}

static inline void
noflush64b(const char *addr)
{
	/* NOP, not even pmemcheck annotation */
}

#ifndef AVX512F_AVAILABLE
/* XXX not supported in MSVC version we currently use */
#ifdef _MSC_VER
#define AVX512F_AVAILABLE 0
#else
#define AVX512F_AVAILABLE 1
#endif
#endif

#ifndef AVX_AVAILABLE
#define AVX_AVAILABLE 1
#endif

#ifndef SSE2_AVAILABLE
#define SSE2_AVAILABLE 1
#endif

#if SSE2_AVAILABLE
void memmove_mov_sse2_clflush(char *dest, const char *src, size_t len);
void memmove_mov_sse2_clflushopt(char *dest, const char *src, size_t len);
void memmove_mov_sse2_clwb(char *dest, const char *src, size_t len);
void memmove_mov_sse2_empty(char *dest, const char *src, size_t len);
void memmove_mov_sse2_noflush(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_clflush(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_clflushopt(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_clwb(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_empty(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_noflush(char *dest, const char *src, size_t len);
void memset_mov_sse2_clflush(char *dest, int c, size_t len);
void memset_mov_sse2_clflushopt(char *dest, int c, size_t len);
void memset_mov_sse2_clwb(char *dest, int c, size_t len);
void memset_mov_sse2_empty(char *dest, int c, size_t len);
void memset_mov_sse2_noflush(char *dest, int c, size_t len);
void memset_movnt_sse2_clflush(char *dest, int c, size_t len);
void memset_movnt_sse2_clflushopt(char *dest, int c, size_t len);
void memset_movnt_sse2_clwb(char *dest, int c, size_t len);
void memset_movnt_sse2_empty(char *dest, int c, size_t len);
void memset_movnt_sse2_noflush(char *dest, int c, size_t len);
#endif

#if AVX_AVAILABLE
void memmove_mov_avx_clflush(char *dest, const char *src, size_t len);
void memmove_mov_avx_clflushopt(char *dest, const char *src, size_t len);
void memmove_mov_avx_clwb(char *dest, const char *src, size_t len);
void memmove_mov_avx_empty(char *dest, const char *src, size_t len);
void memmove_mov_avx_noflush(char *dest, const char *src, size_t len);
void memmove_movnt_avx_clflush(char *dest, const char *src, size_t len);
void memmove_movnt_avx_clflushopt(char *dest, const char *src, size_t len);
void memmove_movnt_avx_clwb(char *dest, const char *src, size_t len);
void memmove_movnt_avx_empty(char *dest, const char *src, size_t len);
void memmove_movnt_avx_noflush(char *dest, const char *src, size_t len);
void memset_mov_avx_clflush(char *dest, int c, size_t len);
void memset_mov_avx_clflushopt(char *dest, int c, size_t len);
void memset_mov_avx_clwb(char *dest, int c, size_t len);
void memset_mov_avx_empty(char *dest, int c, size_t len);
void memset_mov_avx_noflush(char *dest, int c, size_t len);
void memset_movnt_avx_clflush(char *dest, int c, size_t len);
void memset_movnt_avx_clflushopt(char *dest, int c, size_t len);
void memset_movnt_avx_clwb(char *dest, int c, size_t len);
void memset_movnt_avx_empty(char *dest, int c, size_t len);
void memset_movnt_avx_noflush(char *dest, int c, size_t len);
#endif

#if AVX512F_AVAILABLE
void memmove_mov_avx512f_clflush(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_clflushopt(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_clwb(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_empty(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_noflush(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_clflush(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_clflushopt(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_clwb(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_empty(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_noflush(char *dest, const char *src, size_t len);
void memset_mov_avx512f_clflush(char *dest, int c, size_t len);
void memset_mov_avx512f_clflushopt(char *dest, int c, size_t len);
void memset_mov_avx512f_clwb(char *dest, int c, size_t len);
void memset_mov_avx512f_empty(char *dest, int c, size_t len);
void memset_mov_avx512f_noflush(char *dest, int c, size_t len);
void memset_movnt_avx512f_clflush(char *dest, int c, size_t len);
void memset_movnt_avx512f_clflushopt(char *dest, int c, size_t len);
void memset_movnt_avx512f_clwb(char *dest, int c, size_t len);
void memset_movnt_avx512f_empty(char *dest, int c, size_t len);
void memset_movnt_avx512f_noflush(char *dest, int c, size_t len);
#endif

extern size_t Movnt_threshold;

#endif
