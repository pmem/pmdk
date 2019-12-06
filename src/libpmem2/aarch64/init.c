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

#include <string.h>

#include "auto_flush.h"
#include "flush.h"
#include "os.h"
#include "out.h"
#include "pmem2_arch.h"
#include "valgrind_internal.h"

/*
 * memmove_nodrain_libc -- (internal) memmove to pmem without hw drain
 */
static void *
memmove_nodrain_libc(void *pmemdest, const void *src, size_t len,
		unsigned flags, struct pmem2_arch_funcs *funcs)
{
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x", pmemdest, src, len,
			flags);

	memmove(pmemdest, src, len);
	pmem2_flush_flags(pmemdest, len, flags, funcs);
	return pmemdest;
}

/*
 * memset_nodrain_libc -- (internal) memset to pmem without hw drain
 */
static void *
memset_nodrain_libc(void *pmemdest, int c, size_t len, unsigned flags,
		struct pmem2_arch_funcs *funcs)
{
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x", pmemdest, c, len,
			flags);

	memset(pmemdest, c, len);
	pmem2_flush_flags(pmemdest, len, flags, funcs);
	return pmemdest;
}

/*
 * predrain_memory_barrier -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_memory_barrier(void)
{
	LOG(15, NULL);
	arm_store_memory_barrier();
}

/*
 * flush_dcache -- (internal) flush the CPU cache, using clwb
 */
static void
flush_dcache(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_dcache_nolog(addr, len);
}

/*
 * flush_empty -- (internal) do not flush the CPU cache
 */
static void
flush_empty(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_empty_nolog(addr, len);
}

/*
 * pmem2_arch_init -- initialize architecture-specific list of pmem operations
 */
void
pmem2_arch_init(struct pmem2_arch_funcs *funcs)
{
	LOG(3, NULL);

	funcs->predrain_fence = predrain_memory_barrier;
	funcs->deep_flush = flush_dcache;
	funcs->memmove_nodrain = memmove_nodrain_generic;
	funcs->memset_nodrain = memset_nodrain_generic;

	char *ptr = os_getenv("PMEM_NO_GENERIC_MEMCPY");
	if (ptr) {
		long long val = atoll(ptr);

		if (val) {
			funcs->memmove_nodrain = memmove_nodrain_libc;
			funcs->memset_nodrain = memset_nodrain_libc;
		}
	}

	int flush;
	char *e = os_getenv("PMEM_NO_FLUSH");
	if (e && (strcmp(e, "1") == 0)) {
		flush = 0;
		LOG(3, "Forced not flushing CPU_cache");
	} else if (e && (strcmp(e, "0") == 0)) {
		flush = 1;
		LOG(3, "Forced flushing CPU_cache");
	} else if (pmem2_auto_flush() == 1) {
		flush = 0;
		LOG(3, "Not flushing CPU_cache, eADR detected");
	} else {
		flush = 1;
		LOG(3, "Flushing CPU cache");
	}

	if (flush)
		funcs->flush = funcs->deep_flush;
	else
		funcs->flush = flush_empty;

	if (funcs->deep_flush == flush_dcache)
		LOG(3, "Synchronize VA to poc for ARM");
	else
		FATAL("invalid deep flush function address");

	if (funcs->flush == flush_empty)
		LOG(3, "not flushing CPU cache");
	else if (funcs->flush != funcs->deep_flush)
		FATAL("invalid flush function address");

	if (funcs->memmove_nodrain == memmove_nodrain_generic)
		LOG(3, "using generic memmove");
	else if (funcs->memmove_nodrain == memmove_nodrain_libc)
		LOG(3, "using libc memmove");
	else
		FATAL("invalid memove_nodrain function address");
}
