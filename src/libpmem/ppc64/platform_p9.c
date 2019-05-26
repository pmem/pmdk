#include <libpmem.h>
#include <errno.h>

#include "out.h"
#include "pmem.h"
#include "cpuinfo.h"
#include "ppc64_ops.h"

static void ppc_predrain_fence(void)
{
	LOG(15, NULL);
	/*
	 * Force a memory barrier to flush out all cache lines
	 */
	asm volatile(
		"lwsync"
		: : : "memory");
}

static void ppc_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p len %zu", addr, size);
	uintptr_t uptr = (uintptr_t) addr;
	uintptr_t end = uptr + size;

	/* round down the address */
	uptr &= ~(ppc_cpuinfo->d_cache_block_size - 1);
	while (uptr < end) {
		/* issue a dcbst instruction for the cache line */
		asm volatile(
			"dcbst 0,%0"
			: :"r"(uptr) : "memory"
			);

		uptr += ppc_cpuinfo->d_cache_block_size;
	}
}

const static struct pmem_funcs ppc_p9_pmem_funcs = {
	.predrain_fence = ppc_predrain_fence,
	.flush = ppc_flush,
	.deep_flush = ppc_flush,

	/* Use generic functions for rest of the callbacks */
	.is_pmem = is_pmem_detect,
	.memmove_nodrain = memmove_nodrain_generic,
	.memset_nodrain = memset_nodrain_generic,
};

static int platform_init(struct pmem_funcs *funcs)
{
	LOG(3, "Initializing Power-9 Platform");

	*funcs = ppc_p9_pmem_funcs;
	return 0;
}

static int platform_probe(const struct cpu_info *cpuinfo)
{
	/* check the PVR for power9 */
	if (PVR_VER(cpuinfo->pvr) == PVR_POWER9) {
		LOG(3, "Detected Power-9 Platform");
		errno = 0;
	} else {
		errno = EINVAL;
	}

	return errno ? -1 : 0;
}

/* Define the platform */
PPC_DEFINE_PLATFORM("POWER9",
		    &platform_probe,
		    &platform_init);
