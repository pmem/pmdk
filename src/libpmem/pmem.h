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
 * pmem.h -- internal definitions for libpmem
 */
#ifndef PMEM_H
#define PMEM_H

#include <stddef.h>
#include "libpmem.h"
#include "util.h"
#include "valgrind_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMEM_LOG_PREFIX "libpmem"
#define PMEM_LOG_LEVEL_VAR "PMEM_LOG_LEVEL"
#define PMEM_LOG_FILE_VAR "PMEM_LOG_FILE"

typedef void (*predrain_fence_func)(void);
typedef void (*flush_func)(const void *, size_t);
typedef int (*is_pmem_func)(const void *addr, size_t len);
typedef void *(*memmove_nodrain_func)(void *pmemdest, const void *src,
		size_t len, unsigned flags);
typedef void *(*memset_nodrain_func)(void *pmemdest, int c, size_t len,
		unsigned flags);

struct pmem_funcs {
	predrain_fence_func predrain_fence;
	flush_func flush;
	is_pmem_func is_pmem;
	memmove_nodrain_func memmove_nodrain;
	memset_nodrain_func memset_nodrain;
	flush_func deep_flush;
};

void pmem_init(void);
void pmem_os_init(void);
void pmem_init_funcs(struct pmem_funcs *funcs);

int is_pmem_detect(const void *addr, size_t len);
void *pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax);

/*
 * flush_empty_nolog -- (internal) do not flush the CPU cache
 */
static force_inline void
flush_empty_nolog(const void *addr, size_t len)
{
	/* NOP, but tell pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, len);
}

/*
 * flush64b_empty -- (internal) do not flush the CPU cache
 */
static force_inline void
flush64b_empty(const char *addr)
{
	/* NOP, but tell pmemcheck about it */
	VALGRIND_DO_FLUSH(addr, 64);
}

/*
 * pmem_flush_flags -- internal wrapper around pmem_flush
 */
static inline void
pmem_flush_flags(const void *addr, size_t len, unsigned flags)
{
	if (!(flags & PMEM_F_MEM_NOFLUSH))
		pmem_flush(addr, len);
}

void *memmove_nodrain_generic(void *pmemdest, const void *src, size_t len,
		unsigned flags);
void *memset_nodrain_generic(void *pmemdest, int c, size_t len, unsigned flags);

#ifdef __cplusplus
}
#endif

#endif
