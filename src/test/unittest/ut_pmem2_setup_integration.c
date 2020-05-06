// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ut_pmem2_setup_integration.h -- libpmem2 setup functions using public API
 * (for integration tests)
 */

#include <libpmem2.h>
#include "ut_pmem2_config.h"
#include "ut_pmem2_setup_integration.h"
#include "ut_pmem2_source.h"
#include "unittest.h"

/*
 * ut_pmem2_prepare_config_integration -- fill pmem2_config in minimal scope
 */
void
ut_pmem2_prepare_config_integration(const char *file, int line,
	const char *func, struct pmem2_config **cfg, struct pmem2_source **src,
	int fd, enum pmem2_granularity granularity)
{
	ut_pmem2_config_new(file, line, func, cfg);
	ut_pmem2_config_set_required_store_granularity(file, line, func, *cfg,
							granularity);

	ut_pmem2_source_from_fd(file, line, func, src, fd);
}
