// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include "libpmem2.h"
#include "out.h"
#include "source.h"

/*
 * pmem2_source_numa_node -- gets the numa node on which a pmem file
 * is located from given source structure
 */
int
pmem2_source_numa_node(const struct pmem2_source *src, int *numa_node)
{
	ERR("Cannot get numa node from source - ndctl is not available");

	return PMEM2_E_NOSUPP;
}
