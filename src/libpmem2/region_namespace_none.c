// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include <sys/sysmacros.h>
#include <fcntl.h>

#include "libpmem2.h"
#include "pmem2_utils.h"

#include "region_namespace.h"
#include "out.h"

/*
 * devdax_get_region_id -- define behavior without ndctl
 */
int
devdax_get_region_id(const os_stat_t *st, unsigned *region_id)
{
	LOG(3, "Cannot read DevDax region id - ndctl unavailable");

	return 0;
}
