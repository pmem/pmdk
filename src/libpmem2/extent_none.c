// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * extent_none.c - fake implementation of the FS extent query API
 */

#include "libpmem2.h"
#include "out.h"
#include "extent.h"

/*
 * pmem2_extents_count -- save number of extents of the file
 *                        in exts->extents_count and
 *                        block size in exts->blksize
 */
int
pmem2_extents_count(int fd, struct extents *exts)
{
	LOG(3, "fd %i extents %p", fd, exts);

	return PMEM2_E_NOSUPP;
}

/*
 * pmem2_extents_get -- get extents of the given file
 */
int
pmem2_extents_get(int fd, struct extents *exts)
{
	LOG(3, "fd %i extents %p", fd, exts);

	return PMEM2_E_NOSUPP;
}
