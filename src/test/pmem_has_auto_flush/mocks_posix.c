/*
 * Copyright 2018, Intel Corporation
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
