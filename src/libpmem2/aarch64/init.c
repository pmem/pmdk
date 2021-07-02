// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2021, Intel Corporation */

#include <string.h>
#include <sys/auxv.h>
#include <asm/hwcap.h>

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
 * flush_poc -- (internal) flush the dcache to Point of Coherency,
 *              available on all ARMv8+.  It does _not_ flush to dimms
 *              on new CPUs, and is ill-specified earlier.
 */
static void
flush_poc(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_poc_nolog(addr, len);
}

/*
 * flush_pop -- (internal) flush the dcache to Point of Persistency,
 *              available on ARMv8.2+
 */
static void
flush_pop(const void *addr, size_t len)
{
	LOG(15, "addr %p len %zu", addr, len);

	flush_pop_nolog(addr, len);
}

/*
 * is_dcpop_available -- (internal) determine dcpop cpuid flag using hwcaps
 */
static int
is_dc_pop_available(void)
{
	LOG(15, NULL);

	/*
	 * Shouldn't ever fail, but if it does, error is reported as -1
	 * which conveniently includes all bits.  We then assume PoP flushes
	 * are required -- safer on any hardware suspected of actually being
	 * capable of pmem, cleanly crashing with SIGILL on old gear.
	 */
	return getauxval(AT_HWCAP) & HWCAP_DCPOP;
}

/*
 * pmem2_arch_init -- initialize architecture-specific list of pmem operations
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, NULL);

	info->fence = memory_barrier;
	if (is_dc_pop_available())
		info->flush = flush_pop;
	else
		info->flush = flush_poc;

	if (info->flush == flush_poc)
		LOG(3, "Synchronize VA to poc for ARM");
	else if (info->flush == flush_pop)
		LOG(3, "Synchronize VA to pop for ARM");
	else
		FATAL("invalid deep flush function address");
}
