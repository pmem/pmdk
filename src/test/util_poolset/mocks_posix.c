// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * mocks_posix.c -- mocked functions used in util_poolset.c (Posix version)
 */

#include "unittest.h"

extern const char *Open_path;
extern os_off_t Fallocate_len;
extern size_t Is_pmem_len;

/*
 * open -- open mock
 */
FUNC_MOCK(open, int, const char *path, int flags, ...)
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

	return _FUNC_REAL(open)(path, flags, mode);
}
FUNC_MOCK_END

/*
 * posix_fallocate -- posix_fallocate mock
 */
FUNC_MOCK(posix_fallocate, int, int fd, os_off_t offset, off_t len)
FUNC_MOCK_RUN_DEFAULT {
	if (Fallocate_len == len) {
		UT_OUT("mocked fallocate: %ju", len);
		return ENOSPC;
	}
	return _FUNC_REAL(posix_fallocate)(fd, offset, len);
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
