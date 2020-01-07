// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, IBM Corporation */
/* Copyright 2019, Intel Corporation */

#include "platform_generic.h"

#include "util.h"
#include "out.h"
#include "pmem2_arch.h"

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

int
platform_init(struct pmem2_arch_info *info)
{
	LOG(3, "Initializing Platform");

	info->fence = ppc_fence;
	info->flush = ppc_flush;

	return 0;
}
