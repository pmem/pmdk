/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
