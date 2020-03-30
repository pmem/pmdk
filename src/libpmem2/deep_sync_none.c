// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * deep_sync_none.c -- deeep_sync functionality
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "deep_sync.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "persist.h"

/*
 * pmem2_sync_cacheline -- performs flush buffer operation
 */
int
pmem2_sync_cacheline(struct pmem2_map *map, void *ptr, size_t size)
{
	size_t len = MIN(Pagesize, size);
	int ret = pmem2_flush_file_buffers_os(map, ptr, len, 1);
	if (ret) {
		LOG(1, "cannot flush buffers addr %p len %zu",
			ptr, len);
		return PMEM2_E_ERRNO;
	}

	return 0;
}

/*
 * pmem2_deep_sync_write --  perform write to deep_flush file
 * on given region_id (Device Dax only)
 */
int
pmem2_deep_sync_write(int region_id)
{
	const char *err =
		"BUG: pmem2_deep_sync_write should never be called on this OS";
	ERR("%s", err);
	ASSERTinfo(0, err);

	/* not supported */
	errno = ENOTSUP;
	return -1;
}
