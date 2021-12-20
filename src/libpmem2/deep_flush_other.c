// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * deep_flush_other.c -- deep_flush functionality
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "deep_flush.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "persist.h"

/*
 * pmem2_deep_flush_dax -- performs flush buffer operation
 */
int
pmem2_deep_flush_dax(struct pmem2_map *map, void *ptr, size_t size)
{
	int ret = pmem2_flush_file_buffers_os(map, ptr, size, 0);
	if (ret) {
		LOG(1, "cannot flush buffers addr %p len %zu", ptr, size);
		return ret;
	}

	return 0;
}

/*
 * pmem2_deep_flush_write --  perform write to deep_flush file
 * on given region_id (Device Dax only)
 */
int
pmem2_deep_flush_write(unsigned region_id)
{
	SUPPRESS_UNUSED(region_id);
	const char *err =
		"BUG: pmem2_deep_flush_write should never be called on this OS";
	ERR("%s", err);
	ASSERTinfo(0, err);

	/* not supported */
	return PMEM2_E_NOSUPP;
}
