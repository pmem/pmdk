/*
 * Copyright 2014-2018, Intel Corporation
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

/*
 * pmem.h -- internal definitions for libpmem
 */
#ifndef PMEM_H
#define PMEM_H

#ifdef __aarch64__
#include "arm_cacheops.h"
#endif

#include <emmintrin.h>
#include <stdint.h>
#include "util.h"

#define PMEM_LOG_PREFIX "libpmem"
#define PMEM_LOG_LEVEL_VAR "PMEM_LOG_LEVEL"
#define PMEM_LOG_FILE_VAR "PMEM_LOG_FILE"
#define FLUSH_ALIGN ((uintptr_t)64)

void pmem_init(void);

int is_pmem_detect(const void *addr, size_t len);
void *pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax);

#ifndef AVX512F_AVAILABLE
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
void memmove_movnt_sse2_clflush(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_clflushopt(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_clwb(char *dest, const char *src, size_t len);
void memmove_movnt_sse2_empty(char *dest, const char *src, size_t len);
void memset_mov_sse2_clflush(char *dest, int c, size_t len);
void memset_mov_sse2_clflushopt(char *dest, int c, size_t len);
void memset_mov_sse2_clwb(char *dest, int c, size_t len);
void memset_mov_sse2_empty(char *dest, int c, size_t len);
void memset_movnt_sse2_clflush(char *dest, int c, size_t len);
void memset_movnt_sse2_clflushopt(char *dest, int c, size_t len);
void memset_movnt_sse2_clwb(char *dest, int c, size_t len);
void memset_movnt_sse2_empty(char *dest, int c, size_t len);
#endif

#if AVX_AVAILABLE
void memmove_mov_avx_clflush(char *dest, const char *src, size_t len);
void memmove_mov_avx_clflushopt(char *dest, const char *src, size_t len);
void memmove_mov_avx_clwb(char *dest, const char *src, size_t len);
void memmove_mov_avx_empty(char *dest, const char *src, size_t len);
void memmove_movnt_avx_clflush(char *dest, const char *src, size_t len);
void memmove_movnt_avx_clflushopt(char *dest, const char *src, size_t len);
void memmove_movnt_avx_clwb(char *dest, const char *src, size_t len);
void memmove_movnt_avx_empty(char *dest, const char *src, size_t len);
void memset_mov_avx_clflush(char *dest, int c, size_t len);
void memset_mov_avx_clflushopt(char *dest, int c, size_t len);
void memset_mov_avx_clwb(char *dest, int c, size_t len);
void memset_mov_avx_empty(char *dest, int c, size_t len);
void memset_movnt_avx_clflush(char *dest, int c, size_t len);
void memset_movnt_avx_clflushopt(char *dest, int c, size_t len);
void memset_movnt_avx_clwb(char *dest, int c, size_t len);
void memset_movnt_avx_empty(char *dest, int c, size_t len);
#endif

#if AVX512F_AVAILABLE
void memmove_mov_avx512f_clflush(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_clflushopt(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_clwb(char *dest, const char *src, size_t len);
void memmove_mov_avx512f_empty(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_clflush(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_clflushopt(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_clwb(char *dest, const char *src, size_t len);
void memmove_movnt_avx512f_empty(char *dest, const char *src, size_t len);
void memset_mov_avx512f_clflush(char *dest, int c, size_t len);
void memset_mov_avx512f_clflushopt(char *dest, int c, size_t len);
void memset_mov_avx512f_clwb(char *dest, int c, size_t len);
void memset_mov_avx512f_empty(char *dest, int c, size_t len);
void memset_movnt_avx512f_clflush(char *dest, int c, size_t len);
void memset_movnt_avx512f_clflushopt(char *dest, int c, size_t len);
void memset_movnt_avx512f_clwb(char *dest, int c, size_t len);
void memset_movnt_avx512f_empty(char *dest, int c, size_t len);
#endif

extern size_t Movnt_threshold;

#if defined(_WIN32) && (NTDDI_VERSION >= NTDDI_WIN10_RS1)
typedef BOOL (WINAPI *PQVM)(
		HANDLE, const void *,
		enum WIN32_MEMORY_INFORMATION_CLASS, PVOID,
		SIZE_T, PSIZE_T);

extern PQVM Func_qvmi;
#endif

#ifdef _MSC_VER
#define pmem_clflushopt _mm_clflushopt
#define pmem_clwb _mm_clwb
#else
/*
 * The x86 memory instructions are new enough that the compiler
 * intrinsic functions are not always available.  The intrinsic
 * functions are defined here in terms of asm statements for now.
 */
#ifndef __aarch64__
#define pmem_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" : "+m" \
		(*(volatile char *)(addr)));
#define pmem_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" \
		(*(volatile char *)(addr)));
#endif /* __aarch64__ */
#endif /* _MSC_VER */

static force_inline void
noflush(const char *addr)
{
}

#ifndef __aarch64__
/*
 * flush_dcache_invalidate_nolog -- flush the CPU cache, using clflush
 */
static force_inline void
flush_dcache_invalidate_nolog(const void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN)
		_mm_clflush((char *)uptr);
}
#endif

#ifdef __aarch64__
/*
 * flush_clflushopt_nolog -- flush the CPU cache, using
 * arm_clean_and_invalidate_va_to_poc (see arm_cacheops.h) {DC CIVAC}
 */
static force_inline void
flush_dcache_invalidate_opt_nolog(const void *addr, size_t len)
{
	uintptr_t uptr;

	arm_data_memory_barrier();
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		arm_clean_and_invalidate_va_to_poc((char *)uptr);
	}
	arm_data_memory_barrier();
}
#else
/*
 * flush_clflushopt_nolog -- flush the CPU cache, using clflushopt
 */
static force_inline void
flush_dcache_invalidate_opt_nolog(const void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		pmem_clflushopt((char *)uptr);
	}
}
#endif

#ifdef __aarch64__
/*
 * flush_dcache_nolog -- flush the CPU cache, using DC CVAC
 */
static force_inline void
flush_dcache_nolog(const void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		arm_clean_va_to_poc((char *)uptr);
	}
}
#else
/*
 * flush_dcache_nolog -- flush the CPU cache, using clwb
 */
static force_inline void
flush_dcache_nolog(const void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		pmem_clwb((char *)uptr);
	}
}
#endif

/*
 * flush_empty_nolog -- (internal) do not flush the CPU cache
 */
static force_inline void
flush_empty_nolog(const void *addr, size_t len)
{
	/* NOP */
}

#endif
