// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2023, Intel Corporation */

/*
 * ut_pmem2_source.h -- utility helper functions for libpmem2 source tests
 */

#include <libpmem2.h>
#include "unittest.h"
#include "ut_pmem2_source.h"
#include "ut_pmem2_utils.h"

/*
 * ut_pmem2_source_from_fd -- sets fd (cannot fail)
 */
void
ut_pmem2_source_from_fd(const char *file, int line, const char *func,
	struct pmem2_source **src, int fd)
{
	int ret = pmem2_source_from_fd(src, fd);
	ut_pmem2_expect_return(file, line, func, ret, 0);
}

void
ut_pmem2_source_from_fh(const char *file, int line, const char *func,
	struct pmem2_source **src, struct FHandle *f)
{
	enum file_handle_type type = ut_fh_get_handle_type(f);
	int ret;
	if (type == FH_FD) {
		int fd = ut_fh_get_fd(file, line, func, f);
		ret = pmem2_source_from_fd(src, fd);
	} else if (type == FH_HANDLE) {
		ut_fatal(file, line, func,
				"FH_HANDLE not supported on !Windows");
	} else {
		ut_fatal(file, line, func,
				"unknown file handle type");
	}
	ut_pmem2_expect_return(file, line, func, ret, 0);
}

void
ut_pmem2_source_alignment(const char *file, int line, const char *func,
	struct pmem2_source *src, size_t *al)
{
	int ret = pmem2_source_alignment(src, al);
	ut_pmem2_expect_return(file, line, func, ret, 0);
}

void
ut_pmem2_source_delete(const char *file, int line, const char *func,
	struct pmem2_source **src)
{
	int ret = pmem2_source_delete(src);
	ut_pmem2_expect_return(file, line, func, ret, 0);

	UT_ASSERTeq(*src, NULL);
}

void
ut_pmem2_source_size(const char *file, int line, const char *func,
	struct pmem2_source *src, size_t *size)
{
	int ret = pmem2_source_size(src, size);
	ut_pmem2_expect_return(file, line, func, ret, 0);
}
