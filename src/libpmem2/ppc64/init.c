// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, IBM Corporation */
/* Copyright 2019-2020, Intel Corporation */

#include <errno.h>

#include "out.h"
#include "pmem2_arch.h"
#include "platform_generic.h"

/*
 * Probe for valid ppc platforms via the 'ppc_platforms' array and perform its
 * initialization.
 */
void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, "libpmem*: PPC64 support");
	LOG(3, "PMDK PPC64 support is currently experimental");
	LOG(3, "Please don't use this library in production environment");

	/* Init platform and to initialize the pmem funcs */
	if (platform_init(info))
		FATAL("Unable to init platform");
}
