// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * ut_pmem2_setup_integration.h -- libpmem2 setup functions using public API
 * (for integration tests)
 */

#ifndef UT_PMEM2_SETUP_INTEGRATION_H
#define UT_PMEM2_SETUP_INTEGRATION_H 1

#include "ut_fh.h"

/* a prepare_config() that can't set wrong value */
#define PMEM2_PREPARE_CONFIG_INTEGRATION(cfg, src, fd, g)		\
	ut_pmem2_prepare_config_integration(				\
		__FILE__, __LINE__, __func__, cfg, src, fd, g)

void ut_pmem2_prepare_config_integration(const char *file, int line,
	const char *func, struct pmem2_config **cfg, struct pmem2_source **src,
	int fd, enum pmem2_granularity granularity);

#endif /* UT_PMEM2_SETUP_INTEGRATION_H */
