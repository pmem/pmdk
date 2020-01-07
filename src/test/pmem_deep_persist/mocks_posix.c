// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2019, Intel Corporation */

/*
 * mocks_posix.c -- redefinitions of open/write functions (Posix implementation)
 */

#include "util.h"
#include "os.h"
#include "unittest.h"

/*
 * open -- open mock because of  Dev DAX without deep_flush
 * sysfs file, eg. DAX on emulated pmem
 */
FUNC_MOCK(os_open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	if (strstr(path, "/sys/bus/nd/devices/region") &&
			strstr(path, "/deep_flush")) {
		UT_OUT("mocked open, path %s", path);
		if (os_access(path, R_OK))
			return 999;
	}

	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	return _FUNC_REAL(os_open)(path, flags, mode);
}
FUNC_MOCK_END

/*
 * write  -- write mock
 */
FUNC_MOCK(write, int, int fd, const void *buffer, size_t count)
FUNC_MOCK_RUN_DEFAULT {
	if (fd == 999) {
		UT_OUT("mocked write, path %d", fd);
		return 1;
	}
	return _FUNC_REAL(write)(fd, buffer, count);
}
FUNC_MOCK_END
