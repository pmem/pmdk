// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * ut_pmem2_config.h -- utility helper functions for libpmem2 config tests
 */

#ifndef UT_PMEM2_CONFIG_H
#define UT_PMEM2_CONFIG_H 1

#include "ut_fh.h"

/* a pmem2_config_new() that can't return NULL */
#define PMEM2_CONFIG_NEW(cfg)						\
	ut_pmem2_config_new(__FILE__, __LINE__, __func__, cfg)

/* a pmem2_config_set_required_store_granularity() doesn't return an error */
#define PMEM2_CONFIG_SET_GRANULARITY(cfg, g)				\
	ut_pmem2_config_set_required_store_granularity			\
	(__FILE__, __LINE__, __func__, cfg, g)

/* a pmem2_config_delete() that can't return NULL */
#define PMEM2_CONFIG_DELETE(cfg)					\
	ut_pmem2_config_delete(__FILE__, __LINE__, __func__, cfg)

void ut_pmem2_config_new(const char *file, int line, const char *func,
	struct pmem2_config **cfg);

void ut_pmem2_config_set_required_store_granularity(const char *file,
	int line, const char *func, struct pmem2_config *cfg,
	enum pmem2_granularity g);

void ut_pmem2_config_delete(const char *file, int line, const char *func,
	struct pmem2_config **cfg);

#endif /* UT_PMEM2_CONFIG_H */
