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
 * device_dax_linux.h -- linux implementation of device dax helper methods
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/file.h>

#include "util.h"
#include "file.h"
#include "mmap.h"
#include "device_dax.h"

#define DEVICE_DAX_PREFIX "/sys/class/dax"
#define MAX_SIZE_LENGTH 64

/*
 * device_dax_is_dax -- checks whether the given path points to a device dax
 */
int
device_dax_is_dax(const char *path)
{
	util_stat_t st;
	int olderrno = errno;
	int ret = 0;

	if (util_stat(path, &st) < 0)
		goto out;

	if (!S_ISCHR(st.st_mode))
		goto out;

	char spath[PATH_MAX];
	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/subsystem",
		major(st.st_rdev), minor(st.st_rdev));

	char npath[PATH_MAX];
	char *rpath = realpath(spath, npath);
	if (rpath == NULL)
		goto out;

	ret = strcmp(DEVICE_DAX_PREFIX, rpath) == 0;

out:
	errno = olderrno;
	return ret;
}

/*
 * device_dax_size -- checks the size of a given dax device
 */
ssize_t
device_dax_size(const char *path)
{
	util_stat_t st;
	int olderrno;

	if (util_stat(path, &st) < 0)
		return -1;

	char spath[PATH_MAX];
	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/size",
		major(st.st_rdev), minor(st.st_rdev));
	int fd = open(spath, O_RDONLY);
	if (fd < 0)
		return -1;

	ssize_t size = -1;

	char sizebuf[MAX_SIZE_LENGTH];
	ssize_t nread;
	if ((nread = read(fd, sizebuf, MAX_SIZE_LENGTH)) < 0)
		goto out;

	sizebuf[nread] = 0; /* null termination */

	char *endptr;

	olderrno = errno;
	errno = 0;

	size = strtoll(sizebuf, &endptr, 0);
	if (endptr == sizebuf || *endptr != '\n' ||
		((size == LLONG_MAX || size == LLONG_MIN) && errno == ERANGE)) {
		size = -1;
		goto out;
	}

	errno = olderrno;

out:
	olderrno = errno;
	(void) close(fd);
	errno = olderrno;

	return size;
}

/*
 * device_dax_zero -- zeroes the entire dax device
 */
int
device_dax_zero(const char *path)
{
	int fd;
	int olderrno;
	int ret = 0;

	if ((fd = open(path, O_RDWR)) < -1)
		return -1;

	ssize_t size = device_dax_size(path);
	if (size < 0) {
		ret = -1;
		goto out;
	}

	void *addr = util_map(fd, (size_t)size, 0, 0);
	if (addr == NULL) {
		ret = -1;
		goto out;
	}

	/* zero initialize the entire device */
	memset(addr, 0, (size_t)size);

	util_unmap(addr, (size_t)size);

out:
	olderrno = errno;
	(void) close(fd);
	errno = olderrno;

	return ret;
}

/*
 * device_dax_map -- maps the entire dax device
 */
void *
device_dax_map(const char *path)
{
	int fd;
	int olderrno;
	void *addr = NULL;

	if ((fd = open(path, O_RDWR)) < -1)
		return NULL;

	ssize_t size = device_dax_size(path);
	if (size < 0)
		goto out;

	addr = util_map(fd, (size_t)size, 0, 0);
	if (addr == NULL)
		goto out;

out:
	olderrno = errno;
	(void) close(fd);
	errno = olderrno;

	return addr;
}
