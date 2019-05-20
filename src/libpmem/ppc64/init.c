#include <libpmem.h>
#include "out.h"
#include "pmem.h"

static void ppc_predrain_fence(void)
{
	LOG(15, NULL);
}

static void ppc_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p len %zu", addr, size);

}

static void ppc_deep_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p len %zu", addr, size);
}

const static struct pmem_funcs ppc64_pmem_funcs = {
	.predrain_fence = ppc_predrain_fence,
	.flush = ppc_flush,
	.deep_flush = ppc_deep_flush,
	.is_pmem = is_pmem_detect,
	.memmove_nodrain = memmove_nodrain_generic,
	.memset_nodrain = memset_nodrain_generic,
};

/*
 * Provide arch specific implementation for pmem function
 */
void
pmem_init_funcs(struct pmem_funcs *funcs)
{
	LOG(3, "libpmem: PPC64 support");
	LOG(3, "PMDK PPC64 support currently is for testing only");
	LOG(3, "Please dont use this library in production environment");
	*funcs = ppc64_pmem_funcs;
}
