// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include "region_namespace.h"
#include "out.h"

/*
 * pmem2_get_region_id -- define behavior without ndctl
 */
int
pmem2_get_region_id(const os_stat_t *st, unsigned *region_id)
{
	LOG(3, "Cannot read region id - ndctl is not available");

	return 0;
}
