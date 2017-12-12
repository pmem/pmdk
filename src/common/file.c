/*
 * Copyright 2014-2017, Intel Corporation
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
#include <sys/mman.h>

#if !defined(_WIN32) && !defined(__FreeBSD__)
#include <sys/sysmacros.h>
#endif

#include "file.h"
#include "os.h"
#include "out.h"
#include "mmap.h"

#define DEVICE_DAX_PREFIX "/sys/class/dax"
#define MAX_SIZE_LENGTH 64

#define DEVICE_DAX_ZERO_LEN (2 * MEGABYTE)

#ifndef _WIN32
/*
 * device_dax_size -- (internal) checks the size of a given dax device
 */
static ssize_t
device_dax_size(const char *path)
{
	LOG(3, "path \"%s\"", path);

	os_stat_t st;
	int olderrno;

	if (os_stat(path, &st) < 0) {
		ERR("!stat \"%s\"", path);
		return -1;
	}

	char spath[PATH_MAX];
	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/size",
		major(st.st_rdev), minor(st.st_rdev));

	LOG(4, "device size path \"%s\"", spath);

	int fd = os_open(spath, O_RDONLY);
	if (fd < 0) {
		ERR("!open \"%s\"", spath);
		return -1;
	}

	ssize_t size = -1;

	char sizebuf[MAX_SIZE_LENGTH + 1];
	ssize_t nread;
	if ((nread = read(fd, sizebuf, MAX_SIZE_LENGTH)) < 0) {
		ERR("!read");
		goto out;
	}

	sizebuf[nread] = 0; /* null termination */

	char *endptr;

	olderrno = errno;
	errno = 0;

	size = strtoll(sizebuf, &endptr, 0);
	if (endptr == sizebuf || *endptr != '\n' ||
	    ((size == LLONG_MAX || size == LLONG_MIN) && errno == ERANGE)) {
		ERR("invalid device size %s", sizebuf);
		size = -1;
		goto out;
	}

	errno = olderrno;

out:
	olderrno = errno;
	(void) os_close(fd);
	errno = olderrno;

	LOG(4, "device size %zu", size);
	return size;
}
#endif

/*
 * util_fd_is_device_dax -- check whether the file descriptor is associated
 *                          with a device dax
 */
int
util_fd_is_device_dax(int fd)
{
	LOG(3, "fd %d", fd);

#ifdef _WIN32
	return 0;
#else
	os_stat_t st;
	int olderrno = errno;
	int ret = 0;

	if (fd < 0) {
		ERR("invalid file descriptor %d", fd);
		goto out;
	}

	if (os_fstat(fd, &st) < 0) {
		ERR("!fstat");
		goto out;
	}

	if (!S_ISCHR(st.st_mode)) {
		LOG(4, "not a character device");
		goto out;
	}

	char spath[PATH_MAX];
	snprintf(spath, PATH_MAX, "/sys/dev/char/%d:%d/subsystem",
		major(st.st_rdev), minor(st.st_rdev));

	LOG(4, "device subsystem path \"%s\"", spath);

	char npath[PATH_MAX];
	char *rpath = realpath(spath, npath);
	if (rpath == NULL) {
		ERR("!realpath \"%s\"", spath);
		goto out;
	}

	ret = strcmp(DEVICE_DAX_PREFIX, rpath) == 0;

out:
	errno = olderrno;
	LOG(4, "returning %d", ret);
	return ret;
#endif

}

/*
 * util_file_is_device_dax -- checks whether the path points to a device dax
 */
int
util_file_is_device_dax(const char *path)
{
	LOG(3, "path \"%s\"", path);

#ifdef _WIN32
	return 0;
#else
	int olderrno = errno;
	int ret = 0;

	if (path == NULL) {
		ERR("invalid (NULL) path");
		goto out;
	}

	int fd = os_open(path, O_RDONLY);
	if (fd < 0) {
		/* not a problem - 'path' may point to non existent file */
		/* LOG(4, "!open \"%s\"", path); */
		goto out;
	}

	ret = util_fd_is_device_dax(fd);
	(void) os_close(fd);

out:
	errno = olderrno;
	LOG(4, "returning %d", ret);
	return ret;
#endif
}

/*
 * util_file_get_size -- returns size of a file
 */
ssize_t
util_file_get_size(const char *path)
{
	LOG(3, "path \"%s\"", path);

#ifndef _WIN32
	if (util_file_is_device_dax(path)) {
		return device_dax_size(path);
	}
#endif

	os_stat_t stbuf;
	if (os_stat(path, &stbuf) < 0) {
		ERR("!stat \"%s\"", path);
		return -1;
	}

	LOG(4, "file length %zu", stbuf.st_size);
	return stbuf.st_size;
}

/*
 * util_file_map_whole -- maps the entire file into memory
 */
void *
util_file_map_whole(const char *path)
{
	LOG(3, "path \"%s\"", path);

	int fd;
	int olderrno;
	void *addr = NULL;

	if ((fd = os_open(path, O_RDWR)) < 0) {
		ERR("!open \"%s\"", path);
		return NULL;
	}

	ssize_t size = util_file_get_size(path);
	if (size < 0) {
		LOG(2, "cannot determine file length \"%s\"", path);
		goto out;
	}

	addr = util_map(fd, (size_t)size, MAP_SHARED, 0, 0);
	if (addr == NULL) {
		LOG(2, "failed to map entire file \"%s\"", path);
		goto out;
	}

out:
	olderrno = errno;
	(void) os_close(fd);
	errno = olderrno;

	return addr;
}

/*
 * util_file_zero -- zeroes the specified region of the file
 */
int
util_file_zero(const char *path, os_off_t off, size_t len)
{
	LOG(3, "path \"%s\" off %ju len %zu", path, off, len);

	int fd;
	int olderrno;
	int ret = 0;

	if ((fd = os_open(path, O_RDWR)) < 0) {
		ERR("!open \"%s\"", path);
		return -1;
	}

	ssize_t size = util_file_get_size(path);
	if (size < 0) {
		LOG(2, "cannot determine file length \"%s\"", path);
		ret = -1;
		goto out;
	}

	if (off > size) {
		LOG(2, "offset beyond file length, %ju > %ju", off, size);
		ret = -1;
		goto out;
	}

	if ((size_t)off + len > (size_t)size) {
		LOG(2, "requested size of write goes beyond the file length, "
					"%zu > %zu", (size_t)off + len, size);
		LOG(4, "adjusting len to %zu", size - off);
		len = (size_t)(size - off);
	}

	void *addr = util_map(fd, (size_t)size, MAP_SHARED, 0, 0);
	if (addr == NULL) {
		LOG(2, "failed to map entire file \"%s\"", path);
		ret = -1;
		goto out;
	}

	/* zero initialize the specified region */
	memset((char *)addr + off, 0, len);

	util_unmap(addr, (size_t)size);

out:
	olderrno = errno;
	(void) os_close(fd);
	errno = olderrno;

	return ret;
}

/*
 * util_file_pwrite -- writes to a file with an offset
 */
ssize_t
util_file_pwrite(const char *path, const void *buffer, size_t size,
	os_off_t offset)
{
	LOG(3, "path \"%s\" buffer %p size %zu offset %ju",
			path, buffer, size, offset);

	if (!util_file_is_device_dax(path)) {
		int fd = util_file_open(path, NULL, 0, O_RDWR);
		if (fd < 0) {
			LOG(2, "failed to open file \"%s\"", path);
			return -1;
		}

		ssize_t write_len = pwrite(fd, buffer, size, offset);
		int olderrno = errno;
		(void) os_close(fd);
		errno = olderrno;
		return write_len;
	}

	ssize_t file_size = util_file_get_size(path);
	if (file_size < 0) {
		LOG(2, "cannot determine file length \"%s\"", path);
		return -1;
	}

	size_t max_size = (size_t)(file_size - offset);
	if (size > max_size) {
		LOG(2, "requested size of write goes beyond the file length, "
			"%zu > %zu", size, max_size);
		LOG(4, "adjusting size to %zu", max_size);
		size = max_size;
	}

	void *addr = util_file_map_whole(path);
	if (addr == NULL) {
		LOG(2, "failed to map entire file \"%s\"", path);
		return -1;
	}

	memcpy(ADDR_SUM(addr, offset), buffer, size);
	util_unmap(addr, (size_t)file_size);
	return (ssize_t)size;
}

/*
 * util_file_pread -- reads from a file with an offset
 */
ssize_t
util_file_pread(const char *path, void *buffer, size_t size,
	os_off_t offset)
{
	LOG(3, "path \"%s\" buffer %p size %zu offset %ju",
			path, buffer, size, offset);

	if (!util_file_is_device_dax(path)) {
		int fd = util_file_open(path, NULL, 0, O_RDONLY);
		if (fd < 0) {
			LOG(2, "failed to open file \"%s\"", path);
			return -1;
		}

		ssize_t read_len = pread(fd, buffer, size, offset);
		int olderrno = errno;
		(void) os_close(fd);
		errno = olderrno;
		return read_len;
	}

	ssize_t file_size = util_file_get_size(path);
	if (file_size < 0) {
		LOG(2, "cannot determine file length \"%s\"", path);
		return -1;
	}

	size_t max_size = (size_t)(file_size - offset);
	if (size > max_size) {
		LOG(2, "requested size of read goes beyond the file length, "
			"%zu > %zu", size, max_size);
		LOG(4, "adjusting size to %zu", max_size);
		size = max_size;
	}

	void *addr = util_file_map_whole(path);
	if (addr == NULL) {
		LOG(2, "failed to map entire file \"%s\"", path);
		return -1;
	}

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
	LOG(3, "path \"%s\" size %zu minsize %zu", path, size, minsize);

	ASSERTne(size, 0);

	if (size < minsize) {
		ERR("size %zu smaller than %zu", size, minsize);
		errno = EINVAL;
		return -1;
	}

	if (((os_off_t)size) < 0) {
		ERR("invalid size (%zu) for os_off_t", size);
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
	if ((fd = os_open(path, flags, mode)) < 0) {
		ERR("!open \"%s\"", path);
		return -1;
	}

	if ((errno = os_posix_fallocate(fd, 0, (os_off_t)size)) != 0) {
		ERR("!posix_fallocate \"%s\", %zu", path, size);
		goto err;
	}

	/* for windows we can't flock until after we fallocate */
	if (os_flock(fd, OS_LOCK_EX | OS_LOCK_NB) < 0) {
		ERR("!flock \"%s\"", path);
		goto err;
	}

	return fd;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (fd != -1)
		(void) os_close(fd);
	os_unlink(path);
	errno = oerrno;
	return -1;
}

/*
 * util_file_open -- open a memory pool file
 */
int
util_file_open(const char *path, size_t *size, size_t minsize, int flags)
{
	LOG(3, "path \"%s\" size %p minsize %zu flags %d", path, size, minsize,
			flags);

	int oerrno;
	int fd;

#ifdef _WIN32
	flags |= O_BINARY;
#endif

	if ((fd = os_open(path, flags)) < 0) {
		ERR("!open \"%s\"", path);
		return -1;
	}

	if (os_flock(fd, OS_LOCK_EX | OS_LOCK_NB) < 0) {
		ERR("!flock \"%s\"", path);
		(void) os_close(fd);
		return -1;
	}

	if (size || minsize) {
		if (size)
			ASSERTeq(*size, 0);

		ssize_t actual_size = util_file_get_size(path);
		if (actual_size < 0) {
			ERR("stat \"%s\": negative size", path);
			errno = EINVAL;
			goto err;
		}

		if ((size_t)actual_size < minsize) {
			ERR("size %zu smaller than %zu",
					(size_t)actual_size, minsize);
			errno = EINVAL;
			goto err;
		}

		if (size) {
			*size = (size_t)actual_size;
			LOG(4, "actual file size %zu", *size);
		}
	}

	return fd;
err:
	oerrno = errno;
	if (os_flock(fd, OS_LOCK_UN))
		ERR("!flock unlock");
	(void) os_close(fd);
	errno = oerrno;
	return -1;
}

/*
 * util_unlink -- unlinks a file or zeroes a device dax
 */
int
util_unlink(const char *path)
{
	LOG(3, "path \"%s\"", path);

	if (util_file_is_device_dax(path)) {
		return util_file_zero(path, 0, DEVICE_DAX_ZERO_LEN);
	} else {
#ifdef _WIN32
		/* on Windows we can not unlink Read-Only files */
		if (os_chmod(path, S_IREAD | S_IWRITE) == -1) {
			ERR("!chmod \"%s\"", path);
			return -1;
		}
#endif
		return os_unlink(path);
	}
}

/*
 * util_unlink_flock -- flocks the file and unlinks it
 *
 * The unlink(2) call on a file which is opened and locked using flock(2)
 * by different process works on linux. Thus in order to forbid removing a
 * pool when in use by different process we need to flock(2) the pool files
 * first before unlinking.
 */
int
util_unlink_flock(const char *path)
{
	LOG(3, "path \"%s\"", path);

#ifdef WIN32
	/*
	 * On Windows it is not possible to unlink the
	 * file if it is flocked.
	 */
	return util_unlink(path);
#else
	int fd = util_file_open(path, NULL, 0, O_RDONLY);
	if (fd < 0) {
		LOG(2, "failed to open file \"%s\"", path);
		return -1;
	}

	int ret = util_unlink(path);

	(void) os_close(fd);

	return ret;
#endif
}
