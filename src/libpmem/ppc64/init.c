#include <libpmem.h>
#include <errno.h>

#include "out.h"
#include "pmem.h"
#include "cpuinfo.h"

/* Holds current cpu context */
static struct cpu_info cpuinfo;

const struct ppc_platform * ppc_platforms[MAX_PPC_PLATFORMS] = { 0 };

/*
 * Probe for valid ppc platforms via the 'ppc_platforms' array
 * and perform its initialization.
 */
void
pmem_init_funcs(struct pmem_funcs *funcs)
{
	int index, ret;
	/* platform that was probed */
	const struct ppc_platform *platform = NULL;

	LOG(3, "libpmem: PPC64 support");
	LOG(3, "PMDK PPC64 support currently is for testing only");
	LOG(3, "Please dont use this library in production environment");

	LOG(3, "Detecting Platform");
	ppc_populate_cpu_info(&cpuinfo);

	/*
	 * Iterate over the list of supported ppc platforms and see if
	 * anyone is supported.
	 */
	LOG(3, "Checking Platform");
	for (index = 0; index < MAX_PPC_PLATFORMS; ++index) {
		const struct ppc_platform *p = ppc_platforms[index];

		if (p == NULL)
			continue;

		LOG(3, "Probing: %s", p->name);

		/* check if the probe/init functions are in place */
		if (p->platform_probe == NULL ||
		    p->platform_init == NULL)
			continue;
		ret = p->platform_probe(&cpuinfo);
		if (ret) {
			if (errno != EINVAL)
				ERR("Unable to probe %s. Error=%d",
				    p->name, errno);
			continue;
		}

		/* Assign to most recently probed platform */
		platform = p;
	}

	if (platform == NULL)
		FATAL("Unable to find any valid platform");

	/* initialize the global cpu pointer */
	ppc_cpuinfo = &cpuinfo;

	/* Init platform and to initilize the pmem funcs */
	ret = platform->platform_init(funcs);
	if (ret)
		FATAL("Unable to init platform %s. Error=%d",
			platform->name, errno);

	LOG(3, "Probed platform '%s'.", platform->name);
}
