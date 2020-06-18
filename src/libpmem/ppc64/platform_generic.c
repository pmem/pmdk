/*
 * Copyright 2019-2020, IBM Corporation
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
#include "platform_generic.h"

#include <libpmem.h>
#include <errno.h>

#include "util.h"
#include "out.h"
#include "pmem.h"
#include "os.h"

/*
 * Older assemblers versions do not support the latest versions of L, e.g.
 * Binutils 2.34.
 * Workaround this by using longs.
 */
#define __SYNC(l) ".long (0x7c0004AC | ((" #l ") << 21))"
#define __DCBF(ra, rb, l) ".long (0x7c0000AC | ((" #l ") << 21)" \
	" | ((" #ra ") << 16) | ((" #rb ") << 11))"

static void
ppc_predrain_fence(void)
{
	LOG(15, NULL);

	/*
	 * Force a memory barrier to flush out all cache lines.
	 * Uses a heavyweight sync in order to guarantee the memory ordering
	 * even with a data cache flush.
	 * According to the POWER ISA 3.1, phwsync (aka. sync (L=4)) is treated
	 * as a hwsync by processors compatible with previous versions of the
	 * POWER ISA.
	 */
	asm volatile(__SYNC(4) : : : "memory");
}

static void
ppc_predrain_fence_empty(void) {
	LOG(15, NULL);
}

static void
ppc_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p size %zu", addr, size);

	uintptr_t uptr = (uintptr_t)addr;
	uintptr_t end = uptr + size;

	/* round down the address */
	uptr &= ~(CACHELINE_SIZE - 1);
	while (uptr < end) {
		/*
		 * Flush the data cache block.
		 * According to the POWER ISA 3.1, dcbstps (aka. dcbf (L=6))
		 * behaves as dcbf (L=0) on previous processors.
		 */
		asm volatile(__DCBF(0, %0, 6) : :"r"(uptr) : "memory");

		uptr += CACHELINE_SIZE;
	}
}

static void
ppc_flush_empty(const void *addr, size_t size)
{
	LOG(15, "addr %p size %zu", addr, size);

	flush_empty_nolog(addr, size);
}

static void
ppc_flush_msync(const void *addr, size_t size) {
	LOG(15, NULL);

	/* ignore return value */
	pmem_msync(addr, size);
}

static struct pmem_funcs ppc_pmem_funcs = {
	.predrain_fence = ppc_predrain_fence,
	.flush = ppc_flush,
	.deep_flush = ppc_flush,

	/* Use generic functions for rest of the callbacks */
	.is_pmem = is_pmem_detect,
	.memmove_nodrain = memmove_nodrain_generic,
	.memset_nodrain = memset_nodrain_generic,
};

int
platform_init(struct pmem_funcs *funcs)
{
	LOG(3, "Initializing Platform");

	/*
	 * Check for no flush options
	 * ppc64 does not have eADR support so it will not even be checked here
	 */
	char *no_flush = os_getenv("PMEM_NO_FLUSH");
	if (no_flush && strncmp(no_flush, "1", 1) == 0) {
		ppc_pmem_funcs.flush = ppc_flush_empty;
		LOG(3, "Forced not flushing CPU_cache");
	}

	/* XXX: valgrind does not recognize powerpc fence instruction */
	if (On_valgrind) {
		pcc_pmem_funcs.predrain_fence = ppc_predrain_fence_empty;
		pcc_pmem_funcs.flush = ppc_flush_msync;
		pcc_pmem_funcs.deep_flush = ppc_flush_msync;
	}

	*funcs = ppc_pmem_funcs;
	return 0;
}
