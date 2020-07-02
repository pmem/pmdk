// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ut_pmem2_map.h -- utility helper functions for libpmem2 map tests
 */

#include <libpmem2.h>
#include "unittest.h"
#include "ut_pmem2_map.h"
#include "ut_pmem2_utils.h"

/*
 * ut_pmem2_map -- allocates map (cannot fail)
 */
void
ut_pmem2_map(const char *file, int line, const char *func,
	struct pmem2_config *cfg, struct pmem2_source *src,
	struct pmem2_map **map)
{
	int ret = pmem2_map(map, cfg, src);
	ut_pmem2_expect_return(file, line, func, ret, 0);
	UT_ASSERTne(*map, NULL);
}
