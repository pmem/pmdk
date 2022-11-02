// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include <string.h>

#include "auto_flush.h"
#include "out.h"
#include "pmem2_arch.h"
#include "loongarch_cacheops.h"

/*
 * memory_barrier -- (internal) issue the fence instruction
 */
static void
loongarch_memory_fence(void)
{
	LOG(15, NULL);
	loongarch_store_memory_barrier();
}

/*
 * The Cache coherency maintenance between the instruction Cache and the data 
 * Cache within the processor core can be implemented by hardware maintenance.
 */
static void
loongarch_flush(const void *addr, size_t len)
{
	SUPPRESS_UNUSED(addr, len);
}

/*
 * pmem2_arch_init -- initialize architecture-specific list of pmem operations
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, NULL);

	info->fence = loongarch_fence;
	info->flush = loongarch_flush;
}
