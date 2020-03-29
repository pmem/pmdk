// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * deep_sync.c -- pmem2_deep_sync implementation
 */

#include <errno.h>
#include <stdlib.h>

#include "libpmem2.h"
#include "deep_sync.h"

/*
 * pmem2_deep_sync -- performs deep sync operation
 */
int
pmem2_deep_sync(struct pmem2_map *map, void *ptr, size_t size)
{
	return PMEM2_E_NOSUPP;
}
