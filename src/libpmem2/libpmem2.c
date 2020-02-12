// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * libpmem2.c -- pmem2 library constructor & destructor
 */

#include "libpmem2.h"

#include "map.h"
#include "out.h"
#include "persist.h"
#include "pmem2.h"
#include "util.h"

/*
 * libpmem2_init -- load-time initialization for libpmem2
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmem2_init(void)
{
	util_init();
	out_init(PMEM2_LOG_PREFIX, PMEM2_LOG_LEVEL_VAR, PMEM2_LOG_FILE_VAR,
			PMEM2_MAJOR_VERSION, PMEM2_MINOR_VERSION);

	LOG(3, NULL);

	pmem2_map_init();
	pmem2_persist_init();
}

/*
 * libpmem2_fini -- libpmem2 cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmem2_fini(void)
{
	LOG(3, NULL);

	pmem2_map_fini();
	out_fini();
}
