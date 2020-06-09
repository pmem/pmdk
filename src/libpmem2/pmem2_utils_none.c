// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include <errno.h>

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "source.h"

/*
 * pmem2_device_dax_alignment -- checks the alignment of a given
 * dax device from given source
 */
int
pmem2_device_dax_alignment(const struct pmem2_source *src, size_t *alignment)
{
	ERR("Cannot read Device Dax alignment - ndctl is not available");

	return PMEM2_E_NOSUPP;
}
