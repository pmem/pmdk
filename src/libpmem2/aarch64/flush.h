/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2020, Intel Corporation */

#ifndef ARM64_FLUSH_H
#define ARM64_FLUSH_H

#include <stdint.h>
#include "arm_cacheops.h"
#include "util.h"

#define FLUSH_ALIGN ((uintptr_t)64)

/*
 * flush_poc_nolog -- flush the CPU cache, using DC CVAC
 */
static force_inline void
flush_poc_nolog(const void *addr, size_t len)
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

/*
 * flush_pop_nolog -- flush the CPU cache, using DC CVAP
 */
static force_inline void
flush_pop_nolog(const void *addr, size_t len)
{
	uintptr_t uptr;

	/*
	 * Loop through cache-line-size (typically 64B) aligned chunks
	 * covering the given range.
	 */
	for (uptr = (uintptr_t)addr & ~(FLUSH_ALIGN - 1);
		uptr < (uintptr_t)addr + len; uptr += FLUSH_ALIGN) {
		arm_clean_va_to_pop((char *)uptr);
	}
}

#endif
