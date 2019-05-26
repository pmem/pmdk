/*
 * Copyright 2019, IBM Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PPC64__CPUINFO_H__
#define __PPC64__CPUINFO_H__

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
extern const struct ppc_platform *ppc_platforms[];

/* Pointer to glocal cpu context */
extern const struct cpu_info *ppc_cpuinfo;

/* Parse and populate the 'cpuinfo' context */
void ppc_populate_cpu_info(struct cpu_info *cpuinfo);

#endif /* __PPC64__CPUINFO_H__ */
