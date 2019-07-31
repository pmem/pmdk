/*
 * Copyright 2014-2019, Intel Corporation
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
 * file.c -- file utilities
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/file.h>

#ifndef _WIN32
#include <sys/sysmacros.h>
#endif

#include "file.h"
#include "out.h"
#include "mmap.h"

#define MAX_SIZE_LENGTH 64

#ifndef _WIN32
/*
 * device_dax_size -- (internal) checks the size of a given dax device
 */
static ssize_t
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

	char sizebuf[MAX_SIZE_LENGTH + 1];
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
#endif

/*
 * util_file_is_device_dax -- checks whether the path points to a device dax
 */
int
util_file_is_device_dax(const char *path)
{
#ifdef _WIN32
	return 0;
#else
	util_stat_t st;
	int olderrno = errno;
	int ret = 0;

	if (path == NULL)
		goto out;

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

	char *basename = strrchr(rpath, '/');
	if (!basename || strcmp("dax", basename + 1) != 0) {
		LOG(3, "%s path does not match device dax prefix path", rpath);
		goto out;
	}
	ret = 1;

out:
	errno = olderrno;
	return ret;
#endif
}

/*
 * util_file_get_size -- returns size of a file
 */
ssize_t
util_file_get_size(const char *path)
{
#ifndef _WIN32
	if (util_file_is_device_dax(path)) {
		return device_dax_size(path);
	}
#endif

	util_stat_t stbuf;
	if (util_stat(path, &stbuf) < 0) {
		ERR("!fstat %s", path);
		return -1;
	}

	return stbuf.st_size;
}

/*
 * util_file_map_whole -- maps the entire file into memory
 */
void *
util_file_map_whole(const char *path)
{
	int fd;
	int olderrno;
	void *addr = NULL;

	if ((fd = open(path, O_RDWR)) < 0)
		return NULL;

	ssize_t size = util_file_get_size(path);
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

/*
 * util_file_zero_whole -- zeroes the entire file
 */
int
util_file_zero_whole(const char *path)
{
	int fd;
	int olderrno;
	int ret = 0;

	if ((fd = open(path, O_RDWR)) < 0)
		return -1;

	ssize_t size = util_file_get_size(path);
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
 * util_file_pwrite -- writes to a file with an offset
 */
ssize_t
util_file_pwrite(const char *path, const void *buffer, size_t size,
	off_t offset)
{
	if (!util_file_is_device_dax(path)) {
		int fd = util_file_open(path, NULL, 0, O_RDWR);
		if (fd < 0)
			return -1;

		ssize_t write_len = pwrite(fd, buffer, size, offset);
		int olderrno = errno;
		(void) close(fd);
		errno = olderrno;
		return write_len;
	}

	ssize_t file_size = util_file_get_size(path);
	if (file_size < 0)
		return -1;

	size_t max_size = (size_t)(file_size - offset);
	if (size > max_size) {
		LOG(1, "Requested size of write goes beyond the mapped memory");
		size = max_size;
	}

	void *addr = util_file_map_whole(path);
	if (addr == NULL)
		return -1;

	memcpy(ADDR_SUM(addr, offset), buffer, size);
	util_unmap(addr, (size_t)file_size);
	return (ssize_t)size;

}

/*
 * util_file_pread -- reads from a file with an offset
 */
ssize_t
util_file_pread(const char *path, void *buffer, size_t size,
	off_t offset)
{
	if (!util_file_is_device_dax(path)) {
		int fd = util_file_open(path, NULL, 0, O_RDONLY);
		if (fd < 0)
			return -1;

		ssize_t read_len = pread(fd, buffer, size, offset);
		int olderrno = errno;
		(void) close(fd);
		errno = olderrno;
		return read_len;
	}

	ssize_t file_size = util_file_get_size(path);
	if (file_size < 0)
		return -1;

	size_t max_size = (size_t)(file_size - offset);
	if (size > max_size) {
		LOG(1, "Requested size of read goes beyond the mapped memory");
		size = max_size;
	}

	void *addr = util_file_map_whole(path);
	if (addr == NULL)
		return -1;

	memcpy(buffer, ADDR_SUM(addr, offset), size);
	util_unmap(addr, (size_t)file_size);
	return (ssize_t)size;
}

/*
 * util_file_create -- create a new memory pool file
 */
int
util_file_create(const char *path, size_t size, size_t minsize)
{
	LOG(3, "path %s size %zu minsize %zu", path, size, minsize);

	ASSERTne(size, 0);

	if (size < minsize) {
		ERR("size %zu smaller than %zu", size, minsize);
		errno = EINVAL;
		return -1;
	}

	if (((off_t)size) < 0) {
		ERR("invalid size (%zu) for off_t", size);
		errno = EFBIG;
		return -1;
	}

	int fd;
	int mode;
	int flags = O_RDWR | O_CREAT | O_EXCL;
#ifndef _WIN32
	mode = 0;
#else
	mode = S_IWRITE | S_IREAD;
	flags |= O_BINARY;
#endif

	/*
	 * Create file without any permission. It will be granted once
	 * initialization completes.
	 */
	if ((fd = open(path, flags, mode)) < 0) {
		ERR("!open %s", path);
		return -1;
	}

	if ((errno = posix_fallocate(fd, 0, (off_t)size)) != 0) {
		ERR("!posix_fallocate");
		goto err;
	}

	/* for windows we can't flock until after we fallocate */
	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		ERR("!flock");
		goto err;
	}

	return fd;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (fd != -1)
		(void) close(fd);
	unlink(path);
	errno = oerrno;
	return -1;
}

/*
 * util_file_open -- open a memory pool file
 */
int
util_file_open(const char *path, size_t *size, size_t minsize, int flags)
{
	LOG(3, "path %s size %p minsize %zu flags %d", path, size, minsize,
			flags);

	int oerrno;
	int fd;

#ifdef _WIN32
	flags |= O_BINARY;
#endif

	if ((fd = open(path, flags)) < 0) {
		ERR("!open %s", path);
		return -1;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		ERR("!flock");
		(void) close(fd);
		return -1;
	}

	if (size || minsize) {
		if (size)
			ASSERTeq(*size, 0);

		ssize_t actual_size = util_file_get_size(path);
		if (actual_size < 0) {
			ERR("stat %s: negative size", path);
			errno = EINVAL;
			goto err;
		}

		if ((size_t)actual_size < minsize) {
			ERR("size %zu smaller than %zu",
					(size_t)actual_size, minsize);
			errno = EINVAL;
			goto err;
		}

		if (size)
			*size = (size_t)actual_size;
	}

	return fd;
err:
	oerrno = errno;
	if (flock(fd, LOCK_UN))
		ERR("!flock unlock");
	(void) close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_unlink -- unlinks a file or zeroes a device dax
 */
int
util_unlink(const char *path)
{
	if (util_file_is_device_dax(path)) {
		return util_file_zero_whole(path);
	} else {
		return unlink(path);
	}
}
