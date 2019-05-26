#ifndef __PPC64__CPUINFO_H__

#include <stdint.h>
#include <sys/types.h>
#include <linux/limits.h>

/* forward declaration */
struct pmem_funcs;

/* Hold information about current cpu specifically cache related */
struct cpu_info {
	size_t d_cache_block_size;
	size_t d_cache_size;

	size_t page_size;
	size_t blocks_per_page;

	uint64_t pvr;
};

/* probe functions for a platform */
struct ppc_platform {
	const char *name;
	int (*platform_probe) (const struct cpu_info *);
	int (*platform_init) (struct pmem_funcs *);
};

/*
 * Macro that generate a gcc constructor function to populate
 * 'ppc_platforms' array
 */
#define PPC_DEFINE_PLATFORM(NAME, PROBE, INIT)			\
static const struct ppc_platform  _plat_ = {			\
	.name = (NAME),						\
	.platform_probe = (PROBE),				\
	.platform_init = (INIT)					\
};								\
								\
void __attribute__((constructor(101)))  __plat_constructor_(void);	\
void									\
__plat_constructor_(void)						\
{									\
int index;								\
for (index = 0; index < MAX_PPC_PLATFORMS; ++index)			\
	if (ppc_platforms[index] == NULL) {				\
		ppc_platforms[index] = &_plat_;				\
		return;							\
	}								\
}

/*
 * Macro that generate a gcc constructor function to populate
 * 'ppc_platforms' array with a priority
 */
#define PPC_DEFINE_PLATFORM_AND_PRIORITY(NAME, PROBE, INIT, PRIORITY)	\
static const struct ppc_platform  _plat_ = {			\
	.name = (NAME),						\
	.platform_probe = (PROBE),				\
	.platform_init = (INIT)					\
};								\
								\
void __attribute__((constructor(PRIORITY)))  __plat_constructor_(void);	\
void									\
__plat_constructor_(void)						\
{									\
int index;								\
for (index = 0; index < MAX_PPC_PLATFORMS; ++index)			\
	if (ppc_platforms[index] == NULL) {				\
		ppc_platforms[index] = &_plat_;				\
		return;							\
	}								\
}

/* Maximum number of PPC platforms we support */
#define MAX_PPC_PLATFORMS 8

/* List of all supported ppc_platforms */
extern const struct ppc_platform * ppc_platforms[];

/* Pointer to glocal cpu context */
extern const struct cpu_info * ppc_cpuinfo;

/* Parse and populate the 'cpuinfo' context */
void ppc_populate_cpu_info(struct cpu_info *cpuinfo);

#endif /* __PPC64__CPUINFO_H__ */
