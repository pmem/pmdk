// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018, Intel Corporation */

/*
 * mocks_posix.c -- mocked functions used in pmem_has_auto_flush.c
 */
#include <fts.h>
#include "fs.h"
#include "unittest.h"

#define BUS_DEVICE_PATH "/sys/bus/nd/devices"

/*
 * open -- open mock
 */
FUNC_MOCK(open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	if (!strstr(path, BUS_DEVICE_PATH))
		return _FUNC_REAL(open)(path, flags, mode);

	const char *prefix = os_getenv("BUS_DEVICE_PATH");
	char path2[PATH_MAX] = { 0 };
	strcat(path2, prefix);
	strcat(path2, path + strlen(BUS_DEVICE_PATH));
	return _FUNC_REAL(open)(path2, flags, mode);
}
FUNC_MOCK_END

struct fs {
	FTS *ft;
	struct fs_entry entry;
};

/*
 * fs_new -- creates fs traversal instance
 */
FUNC_MOCK(fs_new, struct fs *, const char *path)
FUNC_MOCK_RUN_DEFAULT {
	if (!strstr(path, BUS_DEVICE_PATH))
		return _FUNC_REAL(fs_new)(path);

	const char *prefix = os_getenv("BUS_DEVICE_PATH");
	char path2[PATH_MAX] = { 0 };
	strcat(path2, prefix);
	strcat(path2, path + strlen(BUS_DEVICE_PATH));
	return _FUNC_REAL(fs_new)(path2);
}
FUNC_MOCK_END

/*
 * os_stat -- os_stat mock to handle sysfs path
 */
FUNC_MOCK(os_stat, int, const char *path, os_stat_t *buf)
FUNC_MOCK_RUN_DEFAULT {
	if (!strstr(path, BUS_DEVICE_PATH))
		return _FUNC_REAL(os_stat)(path, buf);

	const char *prefix = os_getenv("BUS_DEVICE_PATH");
	char path2[PATH_MAX] = { 0 };
	strcat(path2, prefix);
	strcat(path2, path + strlen(BUS_DEVICE_PATH));
	return _FUNC_REAL(os_stat)(path2, buf);
}
FUNC_MOCK_END
