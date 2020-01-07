// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2020, Intel Corporation */

/*
 * mocks_windows.c -- mocked functions used in util_poolset.c
 */

#include "pmem.h"
#include "util.h"
#include "unittest.h"

extern const char *Open_path;
extern os_off_t Fallocate_len;
extern size_t Is_pmem_len;

/*
 * os_open -- open mock
 *
 * due to differences in function mocking on linux we are wrapping os_open
 * but on linux we just wrap open syscall
 */
FUNC_MOCK(os_open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	if (strcmp(Open_path, path) == 0) {
		UT_OUT("mocked open: %s", path);
		errno = EACCES;
		return -1;
	}

	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	return _FUNC_REAL(os_open)(path, flags, mode);
}
FUNC_MOCK_END

/*
 * posix_fallocate -- posix_fallocate mock
 */
FUNC_MOCK(os_posix_fallocate, int, int fd, os_off_t offset, os_off_t len)
FUNC_MOCK_RUN_DEFAULT {
	if (Fallocate_len == len) {
		UT_OUT("mocked fallocate: %ju", len);
		return ENOSPC;
	}
	return _FUNC_REAL(os_posix_fallocate)(fd, offset, len);
}
FUNC_MOCK_END

/*
 * pmem_is_pmem -- pmem_is_pmem mock
 */
FUNC_MOCK(pmem_is_pmem, int, const void *addr, size_t len)
FUNC_MOCK_RUN_DEFAULT {
	if (Is_pmem_len == len) {
		UT_OUT("mocked pmem_is_pmem: %zu", len);
		return 1;
	}
	return _FUNC_REAL(pmem_is_pmem)(addr, len);
}
FUNC_MOCK_END

/*
 * On Windows libpmem is statically linked to util_poolset test, but we
 * don't want its ctor to initialize 'out' module.
 */

/*
 * libpmem_init -- load-time initialization for libpmem
 *
 * Called automatically by the run-time loader.
 */
CONSTRUCTOR(libpmem_init)
void
libpmem_init(void)
{
	pmem_init();
}
