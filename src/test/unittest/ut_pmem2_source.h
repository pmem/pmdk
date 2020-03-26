// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, n */

/*
 * ut_pmem2_source.h -- utility helper functions for libpmem2 source tests
 */

#ifndef UT_PMEM2_SOURCE_H
#define UT_PMEM2_SOURCE_H 1

#include "ut_fh.h"

/* a pmem2_config_set_fd() that can't return NULL */
#define PMEM2_SOURCE_FROM_FD(src, fd)					\
	ut_pmem2_source_from_fd(__FILE__, __LINE__, __func__, src, fd)

/* a pmem2_config_set_fd() that can't return NULL */
#define PMEM2_SOURCE_FROM_FH(src, fh)					\
	ut_pmem2_source_from_fh(__FILE__, __LINE__, __func__, src, fh)

/* a pmem2_source_alignment() that can't return an error */
#define PMEM2_SOURCE_ALIGNMENT(src, al)					\
	ut_pmem2_source_alignment(__FILE__, __LINE__, __func__, src, al)

/* a pmem2_source_delete() that can't return NULL */
#define PMEM2_SOURCE_DELETE(src)					\
	ut_pmem2_source_delete(__FILE__, __LINE__, __func__, src)

/* a pmem2_source_source() that can't return NULL */
#define PMEM2_SOURCE_SIZE(src, size)					\
	ut_pmem2_source_size(__FILE__, __LINE__, __func__, src, size)

void ut_pmem2_source_from_fd(const char *file, int line, const char *func,
	struct pmem2_source **src, int fd);

void ut_pmem2_source_from_fh(const char *file, int line, const char *func,
	struct pmem2_source **src, struct FHandle *fhandle);

void ut_pmem2_source_alignment(const char *file, int line, const char *func,
	struct pmem2_source *src, size_t *alignment);

void ut_pmem2_source_delete(const char *file, int line, const char *func,
	struct pmem2_source **src);

void ut_pmem2_source_size(const char *file, int line, const char *func,
	struct pmem2_source *src, size_t *size);

#endif /* UT_PMEM2_SOURCE_H */
