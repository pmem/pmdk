/*
 * Copyright 2016, Intel Corporation
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
 * pmem_provider.c -- unit test for pmem_provider
 */

#include <stdlib.h>
#include <dlfcn.h>
#include "pmem_provider.h"
#include "unittest.h"

#define TEST_PATH "/foo/bar"

int test_running = 0;

int stat_ret = 0;
int stat_errno = 0;
int stat_mode = S_IFREG;
int stat_minor = 0;
int stat_major = 0;
const char *stat_path;
FUNC_MOCK(stat, int, const char *path, struct stat *buf)
	FUNC_MOCK_RUN_DEFAULT {
		stat_path = path;

		buf->st_mode = stat_mode;
		buf->st_rdev = makedev(stat_major, stat_minor);

		errno = stat_errno;
		return stat_ret;
	}
FUNC_MOCK_END

int open_ret = 0;
int open_errno = 0;
const char *open_path;

#define TEST_OPEN_SIZE_PATH "/sys/dev/char/5:10/size"

FUNC_MOCK(open, int, const char *path, int flags, mode_t mode)
	FUNC_MOCK_RUN_DEFAULT {
		if (!test_running)
			return _FUNC_REAL(open)(path, flags, mode);

		open_path = path;

		errno = open_errno;
		return open_ret;
	}
FUNC_MOCK_END

FUNC_MOCK(close, int, int fd)
	FUNC_MOCK_RUN_DEFAULT {
		if (!test_running)
			return _FUNC_REAL(close)(fd);

		return 0;
	}
FUNC_MOCK_END

#define TEST_REALPATH "/sys/dev/char/5:10/subsystem"

const char *realpath_path;
char *realpath_ret = NULL;

FUNC_MOCK(realpath, char *, const char *path, char *resolved_path)
	FUNC_MOCK_RUN_DEFAULT {
		realpath_path = path;

		strcpy(resolved_path, realpath_ret);
		return resolved_path;
	}
FUNC_MOCK_END

int read_ret;
#define TEST_DEVICE_DAX_SIZE 12345
FUNC_MOCK(read, ssize_t, int fd, void *buf, size_t count)
	FUNC_MOCK_RUN_DEFAULT {
		if (!test_running)
			return _FUNC_REAL(read)(fd, buf, count);

		snprintf(buf, count, "%d\n", TEST_DEVICE_DAX_SIZE);

		return read_ret;
	}
FUNC_MOCK_END

static void
test_provider_regular_file_positive()
{
	int ret;
	struct pmem_provider p;

	stat_ret = 0;
	stat_mode = S_IFREG;
	ret = pmem_provider_init(&p, TEST_PATH);
	UT_ASSERTeq(strcmp(stat_path, TEST_PATH), 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(p.pops, NULL);
	UT_ASSERTeq(strcmp(p.path, TEST_PATH), 0);

	ret = p.pops->open(&p, O_RDWR, 0666, 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(strcmp(open_path, TEST_PATH), 0);

	pmem_provider_fini(&p);
}

static void
test_provider_device_dax_positive()
{
	int ret;
	struct pmem_provider p;

	stat_ret = 0;
	stat_mode = S_IFCHR;
	stat_major = 5;
	stat_minor = 10;
	realpath_ret = "/sys/class/dax";
	ret = pmem_provider_init(&p, TEST_PATH);
	UT_ASSERTeq(strcmp(stat_path, TEST_PATH), 0);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTne(p.pops, NULL);
	UT_ASSERTeq(strcmp(p.path, TEST_PATH), 0);
	UT_ASSERTeq(strcmp(realpath_path, TEST_REALPATH), 0);

	ret = p.pops->open(&p, O_RDWR, 0666, 1);
	UT_ASSERTeq(ret, -1);

	ret = p.pops->open(&p, O_RDWR, 0666, 0);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(strcmp(open_path, TEST_PATH), 0);

	ssize_t size = p.pops->get_size(&p);
	UT_ASSERTeq(size, TEST_DEVICE_DAX_SIZE);
	UT_ASSERTeq(strcmp(open_path, TEST_OPEN_SIZE_PATH), 0);

	pmem_provider_fini(&p);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem_provider");
	test_running = 1;

	int ret;
	struct pmem_provider p;
	stat_ret = -1;
	ret = pmem_provider_init(&p, TEST_PATH);
	UT_ASSERTeq(ret, -1);

	test_provider_regular_file_positive();
	test_provider_device_dax_positive();
	test_running = 0;

	DONE(NULL);
}
