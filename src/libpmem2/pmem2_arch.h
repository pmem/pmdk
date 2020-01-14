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

/*
 * pmem2_arch.h -- core-arch interface
 */
#ifndef PMEM2_ARCH_H
#define PMEM2_ARCH_H

#include <stddef.h>
#include "libpmem2.h"
#include "util.h"
#include "valgrind_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

struct pmem2_arch_info;

typedef void (*fence_func)(void);
typedef void (*flush_func)(const void *, size_t);
typedef void *(*memmove_nodrain_func)(void *pmemdest, const void *src,
		size_t len, unsigned flags, flush_func flush);
typedef void *(*memset_nodrain_func)(void *pmemdest, int c, size_t len,
		unsigned flags, flush_func flush);

struct pmem2_arch_info {
	memmove_nodrain_func memmove_nodrain;
	memset_nodrain_func memset_nodrain;
	flush_func flush;
	fence_func fence;
	int flush_has_builtin_fence;
};

void pmem2_arch_init(struct pmem2_arch_info *info);

/*
 * flush_empty_nolog -- (internal) do not flush the CPU cache
 */
static force_inline void
flush_empty_nolog(const void *addr, size_t len)
{
	/* NOP, but tell pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, len);
}

void *memmove_nodrain_generic(void *pmemdest, const void *src, size_t len,
		unsigned flags, flush_func flush);
void *memset_nodrain_generic(void *pmemdest, int c, size_t len, unsigned flags,
		flush_func flush);

#ifdef __cplusplus
}
#endif

#endif
