// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

#ifndef MEMCPY_MEMSET_H
#define MEMCPY_MEMSET_H

#include <stddef.h>
#include <xmmintrin.h>
#include "pmem2_arch.h"

typedef void barrier_fn(void);
typedef void flush64b_fn(const void *);

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
noflush64b(const void *addr)
{
	/* NOP, not even pmemcheck annotation */
}

typedef void perf_barrier_fn(void);

static force_inline void
wc_barrier(void)
{
	/*
	 * Currently, for SSE2 and AVX code paths, use of non-temporal stores
	 * on all generations of CPUs must be limited to the number of
	 * write-combining buffers (12) because otherwise, suboptimal eviction
	 * policy might impact performance when writing more data than WC
	 * buffers can simultaneously hold.
	 *
	 * The AVX512 code path is not affected, probably because we are
	 * overwriting whole cache lines.
	 */
	_mm_sfence();
}

static force_inline void
no_barrier(void)
{
}

#ifndef AVX512F_AVAILABLE
/*
 * XXX not supported in MSVC version we currently use.
 * Enable Windows tests pmem2_mem_ext when MSVC we
 * use will support AVX512F.
 */
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

void memmove_movnt_sse2_clflush_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_clflushopt_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_clwb_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_empty_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_noflush_nobarrier(char *dest, const char *src,
		size_t len);

void memmove_movnt_sse2_clflush_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_clflushopt_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_clwb_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_empty_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_sse2_noflush_wcbarrier(char *dest, const char *src,
		size_t len);

void memset_mov_sse2_clflush(char *dest, int c, size_t len);
void memset_mov_sse2_clflushopt(char *dest, int c, size_t len);
void memset_mov_sse2_clwb(char *dest, int c, size_t len);
void memset_mov_sse2_empty(char *dest, int c, size_t len);
void memset_mov_sse2_noflush(char *dest, int c, size_t len);

void memset_movnt_sse2_clflush_nobarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_clflushopt_nobarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_clwb_nobarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_empty_nobarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_noflush_nobarrier(char *dest, int c, size_t len);

void memset_movnt_sse2_clflush_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_clflushopt_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_clwb_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_empty_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_sse2_noflush_wcbarrier(char *dest, int c, size_t len);
#endif

#if AVX_AVAILABLE
void memmove_mov_avx_clflush(char *dest, const char *src, size_t len);
void memmove_mov_avx_clflushopt(char *dest, const char *src, size_t len);
void memmove_mov_avx_clwb(char *dest, const char *src, size_t len);
void memmove_mov_avx_empty(char *dest, const char *src, size_t len);
void memmove_mov_avx_noflush(char *dest, const char *src, size_t len);

void memmove_movnt_avx_clflush_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_clflushopt_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_clwb_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_empty_nobarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_noflush_nobarrier(char *dest, const char *src,
		size_t len);

void memmove_movnt_avx_clflush_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_clflushopt_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_clwb_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_empty_wcbarrier(char *dest, const char *src,
		size_t len);
void memmove_movnt_avx_noflush_wcbarrier(char *dest, const char *src,
		size_t len);

void memset_mov_avx_clflush(char *dest, int c, size_t len);
void memset_mov_avx_clflushopt(char *dest, int c, size_t len);
void memset_mov_avx_clwb(char *dest, int c, size_t len);
void memset_mov_avx_empty(char *dest, int c, size_t len);
void memset_mov_avx_noflush(char *dest, int c, size_t len);

void memset_movnt_avx_clflush_nobarrier(char *dest, int c, size_t len);
void memset_movnt_avx_clflushopt_nobarrier(char *dest, int c, size_t len);
void memset_movnt_avx_clwb_nobarrier(char *dest, int c, size_t len);
void memset_movnt_avx_empty_nobarrier(char *dest, int c, size_t len);
void memset_movnt_avx_noflush_nobarrier(char *dest, int c, size_t len);

void memset_movnt_avx_clflush_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_avx_clflushopt_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_avx_clwb_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_avx_empty_wcbarrier(char *dest, int c, size_t len);
void memset_movnt_avx_noflush_wcbarrier(char *dest, int c, size_t len);
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

/*
 * SSE2/AVX1 only:
 *
 * How much data WC buffers can hold at the same time, after which sfence
 * is needed to flush them.
 *
 * For some reason sfence affects performance of reading from DRAM, so we have
 * to prefetch the source data earlier.
 */
#define PERF_BARRIER_SIZE (12 * CACHELINE_SIZE /*  768 */)

/*
 * How much to prefetch initially.
 * Cannot be bigger than the size of L1 (32kB) - PERF_BARRIER_SIZE.
 */
#define INI_PREFETCH_SIZE (64 * CACHELINE_SIZE /* 4096 */)

static force_inline void
prefetch(const char *addr)
{
	_mm_prefetch(addr, _MM_HINT_T0);
}

static force_inline void
prefetch_ini_fw(const char *src, size_t len)
{
	size_t pref = MIN(len, INI_PREFETCH_SIZE);
	for (size_t i = 0; i < pref; i += CACHELINE_SIZE)
		prefetch(src + i);
}

static force_inline void
prefetch_ini_bw(const char *src, size_t len)
{
	size_t pref = MIN(len, INI_PREFETCH_SIZE);
	for (size_t i = 0; i < pref; i += CACHELINE_SIZE)
		prefetch(src - i);
}

static force_inline void
prefetch_next_fw(const char *src, const char *srcend)
{
	const char *begin = src + INI_PREFETCH_SIZE;
	const char *end = begin + PERF_BARRIER_SIZE;
	if (end > srcend)
		end = srcend;

	for (const char *addr = begin; addr < end; addr += CACHELINE_SIZE)
		prefetch(addr);
}

static force_inline void
prefetch_next_bw(const char *src, const char *srcbegin)
{
	const char *begin = src - INI_PREFETCH_SIZE;
	const char *end = begin - PERF_BARRIER_SIZE;
	if (end < srcbegin)
		end = srcbegin;

	for (const char *addr = begin; addr >= end; addr -= CACHELINE_SIZE)
		prefetch(addr);
}

#endif
