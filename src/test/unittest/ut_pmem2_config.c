// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * ut_pmem2_config.h -- utility helper functions for libpmem2 config tests
 */

#include <libpmem2.h>
#include "unittest.h"
#include "ut_pmem2_config.h"
#include "ut_pmem2_utils.h"
#include "config.h"

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
 * ut_pmem2_config_set_fd -- sets fd (cannot fail)
 */
void
ut_pmem2_config_set_fd(const char *file, int line, const char *func,
	struct pmem2_config *cfg, int fd)
{
	int ret = pmem2_config_set_fd(cfg, fd);
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
 * ut_pmem2_config_set_fhandle -- stores FHandle in the config structure
 */
void
ut_pmem2_config_set_fhandle(const char *file, int line, const char *func,
		struct pmem2_config *cfg, struct FHandle *f)
{
	enum file_handle_type type = ut_fh_get_handle_type(f);
	if (type == FH_FD) {
		int fd = ut_fh_get_fd(file, line, func, f);

#ifdef _WIN32
		cfg->handle = (HANDLE)_get_osfhandle(fd);
#else
		cfg->fd = fd;
#endif
	} else if (type == FH_HANDLE) {
#ifdef _WIN32
		cfg->handle = ut_fh_get_handle(file, line, func, f);
#else
		ut_fatal(file, line, func,
				"FH_HANDLE not supported on !Windows");
#endif
	} else {
		ut_fatal(file, line, func,
				"unknown file handle type");
	}
}
