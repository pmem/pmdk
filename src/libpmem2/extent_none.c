// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * extent_none.c - fake implementation of the FS extent query API
 */

#include "libpmem2.h"
#include "out.h"
#include "extent.h"

/*
 * pmem2_extents_create_get -- allocate extents structure and get extents
 *                             of the given file
 */
int
pmem2_extents_create_get(int fd, struct extents **exts)
{
	LOG(3, "fd %i extents %p", fd, exts);

	return PMEM2_E_NOSUPP;
}

/*
 * pmem2_extents_destroy -- free extents structure
 */
void
pmem2_extents_destroy(struct extents **exts)
{
	LOG(3, "extents %p", exts);
}
