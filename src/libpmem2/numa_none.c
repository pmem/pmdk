// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

#include "libpmem2.h"
#include "util.h"
#include "log_internal.h"
#include "source.h"

/*
 * pmem2_source_numa_node -- gets the numa node on which a pmem file
 * is located from given source structure
 */
int
pmem2_source_numa_node(const struct pmem2_source *src, int *numa_node)
{
	SUPPRESS_UNUSED(src, numa_node);
	ERR_WO_ERRNO(
		"Cannot get numa node from source - ndctl is not available");

	return PMEM2_E_NOSUPP;
}
