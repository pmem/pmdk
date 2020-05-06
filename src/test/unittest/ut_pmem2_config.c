// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * ut_pmem2_config.h -- utility helper functions for libpmem2 config tests
 */

#include <libpmem2.h>
#include "unittest.h"
#include "ut_pmem2_config.h"
#include "ut_pmem2_source.h"
#include "ut_pmem2_utils.h"

/*
 * ut_pmem2_config_new -- allocates cfg (cannot fail)
 */
void
ut_pmem2_config_new(const char *file, int line, const char *func,
	struct pmem2_config **cfg)
{
	int ret = pmem2_config_new(cfg);
	ut_pmem2_expect_return(file, line, func, ret, 0);

	UT_ASSERTne(*cfg, NULL);
}

/*
 * pmem2_config_set_required_store_granularity -- sets granularity
 */
void
ut_pmem2_config_set_required_store_granularity(const char *file, int line,
	const char *func, struct pmem2_config *cfg, enum pmem2_granularity g)
{
	int ret = pmem2_config_set_required_store_granularity(cfg, g);
	ut_pmem2_expect_return(file, line, func, ret, 0);
}

/*
 * ut_pmem2_config_delete -- deallocates cfg (cannot fail)
 */
void
ut_pmem2_config_delete(const char *file, int line, const char *func,
	struct pmem2_config **cfg)
{
	int ret = pmem2_config_delete(cfg);
	ut_pmem2_expect_return(file, line, func, ret, 0);

	UT_ASSERTeq(*cfg, NULL);
}

/*
 * prepare_config -- fill pmem2_config in minimal scope
 */
void
ut_prepare_config(const char *file, int line, const char *func, 
	struct pmem2_config **cfg, struct pmem2_source **src, int fd,
	enum pmem2_granularity granularity)
{
	ut_pmem2_config_new(file, line, func, cfg);

	ut_pmem2_source_from_fd(file, line, func, src, fd);

	ut_pmem2_config_set_required_store_granularity(file, line, func, *cfg,
							granularity);
}