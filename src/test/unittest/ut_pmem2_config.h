// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * ut_pmem2_config.h -- utility helper functions for libpmem2 config tests
 */

#ifndef UT_PMEM2_CONFIG_H
#define UT_PMEM2_CONFIG_H 1

#include "ut_fh.h"

/* a pmem2_config_new() that can't return NULL */
#define PMEM2_CONFIG_NEW(cfg)						\
	ut_pmem2_config_new(__FILE__, __LINE__, __func__, cfg)

/* a pmem2_config_set_fd() that can't return NULL */
#define PMEM2_CONFIG_SET_FD(cfg, fd)					\
	ut_pmem2_config_set_fd(__FILE__, __LINE__, __func__, cfg, fd)

/* a pmem2_config_delete() that can't return NULL */
#define PMEM2_CONFIG_DELETE(cfg)					\
	ut_pmem2_config_delete(__FILE__, __LINE__, __func__, cfg)

/* generic pmem2_config_set_fd/handle() for FHandles */
#define PMEM2_CONFIG_SET_FHANDLE(cfg, fh)				\
	ut_pmem2_config_set_fhandle(__FILE__, __LINE__, __func__, cfg, fh)

void ut_pmem2_config_new(const char *file, int line, const char *func,
	struct pmem2_config **cfg);

void ut_pmem2_config_set_fd(const char *file, int line, const char *func,
	struct pmem2_config *cfg, int fd);

void ut_pmem2_config_delete(const char *file, int line, const char *func,
	struct pmem2_config **cfg);

void ut_pmem2_config_set_fhandle(const char *file, int line, const char *func,
		struct pmem2_config *cfg, struct FHandle *f);

#endif /* UT_PMEM2_CONFIG_H */
