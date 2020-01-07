// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * mocks_posix.c -- mocked functions used in auto_flush_linux.c
 */
#include <fts.h>
#include "map.h"
#include "../common/mmap.h"
#include "fs.h"
#include "unittest.h"

#define BUS_DEVICE_PATH "/sys/bus/nd/devices"

/*
 * mmap - mock mmap
 */
FUNC_MOCK(mmap, void *, void *addr, size_t len, int prot,
		int flags, int fd, __off_t offset)
FUNC_MOCK_RUN_DEFAULT {
	char *str_map_sync = os_getenv("IS_PMEM");
	const int ms = MAP_SYNC | MAP_SHARED_VALIDATE;
	int map_sync_try = ((flags & ms) == ms) ? 1 : 0;
	if (str_map_sync && atoi(str_map_sync) == 1) {
		if (map_sync_try) {
			flags &= ~ms;
			flags |= MAP_SHARED;
			return _FUNC_REAL(mmap)(addr, len, prot, flags,
				fd, offset);
		}
	} else if (map_sync_try) {
		errno = EINVAL;
		return MAP_FAILED;
	}

	return _FUNC_REAL(mmap)(addr, len, prot, flags, fd, offset);
}
FUNC_MOCK_END

/*
 * open -- open mock
 */
FUNC_MOCK(open, int, const char *path, int flags, ...)
FUNC_MOCK_RUN_DEFAULT {
	va_list ap;
	va_start(ap, flags);
	int mode = va_arg(ap, int);
	va_end(ap);

	char *is_bus_device_path = strstr(path, BUS_DEVICE_PATH);
	if (!is_bus_device_path ||
		(is_bus_device_path && strstr(path, "region")))
		return _FUNC_REAL(open)(path, flags, mode);

	const char *mock_path = os_getenv("BUS_DEVICE_PATH");
	return _FUNC_REAL(open)(mock_path, flags, mode);
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
	char *is_bus_device_path = strstr(path, BUS_DEVICE_PATH);
	if (!is_bus_device_path ||
		(is_bus_device_path && strstr(path, "region")))
		return _FUNC_REAL(fs_new)(path);

	const char *mock_path = os_getenv("BUS_DEVICE_PATH");
	return _FUNC_REAL(fs_new)(mock_path);
}
FUNC_MOCK_END

/*
 * os_stat -- os_stat mock to handle sysfs path
 */
FUNC_MOCK(os_stat, int, const char *path, os_stat_t *buf)
FUNC_MOCK_RUN_DEFAULT {
	char *is_bus_device_path = strstr(path, BUS_DEVICE_PATH);
	if (!is_bus_device_path ||
		(is_bus_device_path && strstr(path, "region")))
		return _FUNC_REAL(os_stat)(path, buf);

	const char *mock_path = os_getenv("BUS_DEVICE_PATH");
	return _FUNC_REAL(os_stat)(mock_path, buf);
}
FUNC_MOCK_END
