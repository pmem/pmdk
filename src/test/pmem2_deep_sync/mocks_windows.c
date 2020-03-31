// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_windows.c -- mocked function required to count
 * persists functions called by pmem2_deep_sync test
 */

#include "map.h"
#include "pmem2_deep_sync.h"
#include "unittest.h"

extern int n_msynces;

FUNC_MOCK(pmem2_flush_file_buffers_os, int,
	struct pmem2_map *map, const void *addr,
	size_t len, int autorestart)
FUNC_MOCK_RUN_DEFAULT {
	n_msynces++;
	return 0;
}
FUNC_MOCK_END

FUNC_MOCK(pmem2_set_flush_fns, void,
	struct pmem2_map *map)
FUNC_MOCK_RUN_DEFAULT {
	map->persist_fn = pmem2_persist_mock;
}
FUNC_MOCK_END
