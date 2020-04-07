// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2020, Intel Corporation */

#ifndef X86_64_FLUSH_H
#define X86_64_FLUSH_H

#include <emmintrin.h>
#include <stddef.h>
#include <stdint.h>
#include "util.h"
#include "valgrind_internal.h"

#define FLUSH_ALIGN ((uintptr_t)64)

static force_inline void
pmem_clflush(const void *addr)
{
	_mm_clflush(addr);
}

#ifdef _MSC_VER
static force_inline void
pmem_clflushopt(const void *addr)
{
	_mm_clflushopt(addr);
}

static force_inline void
pmem_clwb(const void *addr)
{
	_mm_clwb(addr);
}
#else
/*
 * The x86 memory instructions are new enough that the compiler
 * intrinsic functions are not always available.  The intrinsic
 * functions are defined here in terms of asm statements for now.
 */
static force_inline void
pmem_clflushopt(const void *addr)
{
	asm volatile(".byte 0x66; clflush %0" : "+m" \
		(*(volatile char *)(addr)));
}
static force_inline void
pmem_clwb(const void *addr)
{
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" \
		(*(volatile char *)(addr)));
}
#endif /* _MSC_VER */

typedef void flush_fn(const void *, size_t);

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
flush64b_empty(const void *addr)
{
	/* NOP, but tell pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, 64);
}

#endif
