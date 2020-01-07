// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

#include <string.h>

#include "auto_flush.h"
#include "flush.h"
#include "out.h"
#include "pmem2_arch.h"

/*
 * memory_barrier -- (internal) issue the fence instruction
 */
static void
memory_barrier(void)
{
	LOG(15, NULL);
	arm_store_memory_barrier();
}

/*
 * flush_dcache -- (internal) flush the CPU cache
 */
static void
flush_dcache(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_dcache_nolog(addr, len);
}

/*
 * pmem2_arch_init -- initialize architecture-specific list of pmem operations
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, NULL);

	info->fence = memory_barrier;
	info->flush = flush_dcache;

	if (info->flush == flush_dcache)
		LOG(3, "Synchronize VA to poc for ARM");
	else
		FATAL("invalid deep flush function address");
}
