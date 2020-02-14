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

/* a pmem2_config_delete() that can't return NULL */
#define PMEM2_CONFIG_DELETE(cfg)					\
	ut_pmem2_config_delete(__FILE__, __LINE__, __func__, cfg)

/* a pmem2_config_set_fd() that can't return NULL */
#define PMEM2_SOURCE_FROM_FD(src, fd)					\
	ut_pmem2_source_from_fd(__FILE__, __LINE__, __func__, src, fd)

/* a pmem2_config_set_fd() that can't return NULL */
#define PMEM2_SOURCE_FROM_FH(src, fh)					\
	ut_pmem2_source_from_fh(__FILE__, __LINE__, __func__, src, fh)

/* a pmem2_source_delete() that can't return NULL */
#define PMEM2_SOURCE_DELETE(src)					\
	ut_pmem2_source_delete(__FILE__, __LINE__, __func__, src)

void ut_pmem2_config_new(const char *file, int line, const char *func,
	struct pmem2_config **cfg);

void ut_pmem2_config_delete(const char *file, int line, const char *func,
	struct pmem2_config **cfg);

void ut_pmem2_source_from_fd(const char *file, int line, const char *func,
	struct pmem2_source **src, int fd);

void ut_pmem2_source_from_fh(const char *file, int line, const char *func,
	struct pmem2_source **src, struct FHandle *fhandle);

void ut_pmem2_source_delete(const char *file, int line, const char *func,
	struct pmem2_source **src);

#endif /* UT_PMEM2_CONFIG_H */
