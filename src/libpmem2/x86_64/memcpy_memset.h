// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

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
