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

#include <string.h>
#include "libpmem.h"

#include "flush.h"
#include "os.h"
#include "out.h"
#include "pmem.h"
#include "valgrind_internal.h"

/*
 * memmove_nodrain_libc -- (internal) memmove to pmem without hw drain
 */
static void *
memmove_nodrain_libc(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	LOG(15, "pmemdest %p src %p len %zu flags 0x%x", pmemdest, src, len,
			flags);

	memmove(pmemdest, src, len);
	pmem_flush_flags(pmemdest, len, flags);
	return pmemdest;
}

/*
 * memset_nodrain_libc -- (internal) memset to pmem without hw drain
 */
static void *
memset_nodrain_libc(void *pmemdest, int c, size_t len, unsigned flags)
{
	LOG(15, "pmemdest %p c 0x%x len %zu flags 0x%x", pmemdest, c, len,
			flags);

	memset(pmemdest, c, len);
	pmem_flush_flags(pmemdest, len, flags);
	return pmemdest;
}

/*
 * predrain_fence_empty -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_fence_empty(void)
{
	LOG(15, NULL);

	VALGRIND_DO_FENCE;
	/* nothing to do (because CLFLUSH did it for us) */
}

/*
 * predrain_memory_barrier -- (internal) issue the pre-drain fence instruction
 */
static void
predrain_memory_barrier(void)
{
	LOG(15, NULL);
	arm_data_memory_barrier();
}

/*
 * flush_dcache_invalidate_opt -- (internal) flush the CPU cache,
 * using clflushopt for X86 and arm_clean_and_invalidate_va_to_poc
 * for aarch64 (see arm_cacheops.h) {DC CIVAC}
 */
static void
flush_dcache_invalidate_opt(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_dcache_invalidate_opt_nolog(addr, len);
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
 * pmem_init_funcs -- initialize architecture-specific list of pmem operations
 */
void
pmem_init_funcs(struct pmem_funcs *funcs)
{
	LOG(3, NULL);

	funcs->predrain_fence = predrain_fence_empty;
	funcs->deep_flush = flush_dcache_invalidate_opt;
	funcs->is_pmem = is_pmem_detect;
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
	} else if (pmem_has_auto_flush() == 1) {
		flush = 0;
		LOG(3, "Not flushing CPU_cache, eADR detected");
	} else {
		flush = 1;
		LOG(3, "Flushing CPU cache");
	}

	if (flush) {
		funcs->flush = funcs->deep_flush;
	} else {
		funcs->flush = flush_empty;
		funcs->predrain_fence = predrain_memory_barrier;
	}

	if (funcs->deep_flush == flush_dcache)
		LOG(3, "Using ARM invalidate");
	else if (funcs->deep_flush == flush_dcache_invalidate_opt)
		LOG(3, "Synchronize VA to poc for ARM");
	else
		FATAL("invalid deep flush function address");

	if (funcs->deep_flush == flush_empty)
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
