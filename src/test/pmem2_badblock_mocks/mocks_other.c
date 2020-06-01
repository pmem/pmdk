// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * mocks_other.c -- mocked various functions used
 *                  indirectly in pmem2_badblock_mocks.c
 */

#include <sys/stat.h>

#include "unittest.h"
#include "out.h"
#include "pmem2_badblock_mocks.h"

/*
 * fallocate -- mock fallocate
 */
FUNC_MOCK(fallocate, int,
		int fd, int mode, __off_t offset, __off_t len)
FUNC_MOCK_RUN_DEFAULT {
	UT_OUT("fallocate(%i, %i, %lu, %lu)", fd, mode, offset, len);
	return 0;
}
FUNC_MOCK_END

/*
 * fcntl -- mock fcntl
 */
FUNC_MOCK(fcntl, int,
		int fildes, int cmd)
FUNC_MOCK_RUN_DEFAULT {
	UT_ASSERTeq(cmd, F_GETFL);
	return O_RDWR;
}
FUNC_MOCK_END
