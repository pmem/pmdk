// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * mocks_windows.c -- mocked functions used in pmem_map_file.c
 *                    (Windows-specific)
 */

#include "unittest.h"

#define MAX_LEN (4 * 1024 * 1024)

/*
 * posix_fallocate -- interpose on libc posix_fallocate()
 */
FUNC_MOCK(os_posix_fallocate, int, int fd, os_off_t offset, os_off_t len)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("posix_fallocate: off %ju len %ju", offset, len);
	if (len > MAX_LEN)
		return ENOSPC;
	return _FUNC_REAL(os_posix_fallocate)(fd, offset, len);
}
FUNC_MOCK_END

/*
 * ftruncate -- interpose on libc ftruncate()
 */
FUNC_MOCK(os_ftruncate, int, int fd, os_off_t len)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("ftruncate: len %ju", len);
	if (len > MAX_LEN) {
		errno = ENOSPC;
		return -1;
	}
	return _FUNC_REAL(os_ftruncate)(fd, len);
}
FUNC_MOCK_END
