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

/*
 * util_pmem.h -- internal definitions for pmem utils
 */

#ifndef PMDK_UTIL_PMEM_H
#define PMDK_UTIL_PMEM_H 1

#include "libpmem.h"
#include "out.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * util_persist -- flush to persistence
 */
static inline void
util_persist(int is_pmem, const void *addr, size_t len)
{
	LOG(3, "is_pmem %d, addr %p, len %zu", is_pmem, addr, len);

	if (is_pmem)
		pmem_persist(addr, len);
	else if (pmem_msync(addr, len))
		FATAL("!pmem_msync");
}

/*
 * util_persist_auto -- flush to persistence
 */
static inline void
util_persist_auto(int is_pmem, const void *addr, size_t len)
{
	LOG(3, "is_pmem %d, addr %p, len %zu", is_pmem, addr, len);

	util_persist(is_pmem || pmem_is_pmem(addr, len), addr, len);
}

#ifdef __cplusplus
}
#endif

#endif /* util_pmem.h */
