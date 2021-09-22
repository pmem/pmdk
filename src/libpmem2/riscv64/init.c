// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

#include <string.h>

#include "auto_flush.h"
#include "out.h"
#include "pmem2_arch.h"
#include "rv_cacheops.h"

/*
 * memory_barrier -- (internal) issue the fence instruction
 */
static void
memory_barrier(void)
{
	LOG(15, NULL);
	riscv_store_memory_barrier();
}

static void
noop(const void *addr, size_t len)
{
}

/*
 * pmem2_arch_init -- initialize architecture-specific list of pmem operations
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, NULL);

	info->fence = memory_barrier;
	info->flush = noop;
}
