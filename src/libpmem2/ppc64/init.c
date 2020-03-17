// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, IBM Corporation */
/* Copyright 2019-2020, Intel Corporation */

#include <errno.h>

#include "out.h"
#include "pmem2_arch.h"
#include "util.h"

static void
ppc_fence(void)
{
	LOG(15, NULL);

	/*
	 * Force a memory barrier to flush out all cache lines
	 */
	asm volatile(
		"lwsync"
		: : : "memory");
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
		/* issue a dcbst instruction for the cache line */
		asm volatile(
			"dcbst 0,%0"
			: :"r"(uptr) : "memory");

		uptr += CACHELINE_SIZE;
	}
}

void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, "libpmem*: PPC64 support");
	LOG(3, "PMDK PPC64 support is currently experimental");
	LOG(3, "Please don't use this library in production environment");

	info->fence = ppc_fence;
	info->flush = ppc_flush;
}
