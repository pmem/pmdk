// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * persist_windows.c -- Windows-specific part of persist implementation
 */

#include <stdlib.h>
#include <windows.h>

#include "out.h"
#include "persist.h"
#include "pmem2_utils.h"

/*
 * pmem2_flush_file_buffers_os -- flush CPU and OS file caches for the given
 * range
 */
int
pmem2_flush_file_buffers_os(struct pmem2_map *map, const void *addr, size_t len,
		int autorestart)
{
	if (FlushViewOfFile(addr, len) == FALSE) {
		ERR("!!FlushViewOfFile");
		return pmem2_lasterror_to_err();
	}

	if (FlushFileBuffers(map->handle) == FALSE) {
		ERR("!!FlushFileBuffers");
		return pmem2_lasterror_to_err();
	}

	return 0;
}
