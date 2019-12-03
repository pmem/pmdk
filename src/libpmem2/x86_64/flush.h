/*
 * Copyright 2014-2019, Intel Corporation
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

#ifndef X86_64_FLUSH_H
#define X86_64_FLUSH_H

#include <emmintrin.h>
#include <stddef.h>
#include <stdint.h>
#include "util.h"

#define FLUSH_ALIGN ((uintptr_t)64)

#ifdef _MSC_VER
#define pmem_clflushopt _mm_clflushopt
#define pmem_clwb _mm_clwb
#else
/*
 * The x86 memory instructions are new enough that the compiler
 * intrinsic functions are not always available.  The intrinsic
 * functions are defined here in terms of asm statements for now.
 */
#define pmem_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" : "+m" \
		(*(volatile char *)(addr)));
#define pmem_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" \
		(*(volatile char *)(addr)));
#endif /* _MSC_VER */

/*
 * flush_clflush_nolog -- flush the CPU cache, using clflush
 */
static force_inline void
flush_clflush_nolog(const void *addr, size_t len)
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

/*
 * flush_clflushopt_nolog -- flush the CPU cache, using clflushopt
 */
static force_inline void
flush_clflushopt_nolog(const void *addr, size_t len)
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

/*
 * flush_clwb_nolog -- flush the CPU cache, using clwb
 */
static force_inline void
flush_clwb_nolog(const void *addr, size_t len)
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

/*
 * flush64b_empty -- (internal) do not flush the CPU cache
 */
static force_inline void
flush64b_empty(const char *addr)
{
}

#endif
