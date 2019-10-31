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
#include <sys/mman.h>

#if !defined(_WIN32) && !defined(__FreeBSD__)
#include <sys/sysmacros.h>
#endif

#include "../libpmem2/config.h"
#include "../libpmem2/pmem2_utils.h"
#include "file.h"
#include "os.h"
#include "out.h"
#include "mmap.h"

#define DEVICE_DAX_ZERO_LEN (2 * MEGABYTE)

/*
 * util_file_exists -- checks whether file exists
 */
int
util_file_exists(const char *path)
{
	LOG(3, "path \"%s\"", path);

	if (os_access(path, F_OK) == 0)
		return 1;

	if (errno != ENOENT) {
		ERR("!os_access \"%s\"", path);
		return -1;
	}

	/*
	 * ENOENT means that some component of a pathname does not exists.
	 *
	 * XXX - we should also call os_access on parent directory and
	 * if this also results in ENOENT -1 should be returned.
	 *
	 * The problem is that we would need to use realpath, which fails
	 * if file does not exist.
	 */

	return 0;
}

/*
 * util_stat_get_type -- checks whether stat structure describes
 *			 device dax or a normal file
 */
enum file_type
util_stat_get_type(const os_stat_t *st)
{
	enum pmem2_file_type type;

	int ret = pmem2_get_type_from_stat(st, &type);
	if (ret) {
		errno = pmem2_err_to_errno(ret);
		return OTHER_ERROR;
	}

	if (type == PMEM2_FTYPE_REG || type == PMEM2_FTYPE_DIR)
		return TYPE_NORMAL;

	if (type == PMEM2_FTYPE_DEVDAX)
		return TYPE_DEVDAX;

	ASSERTinfo(0, "unhandled file type in util_stat_get_type");
	return OTHER_ERROR;
}

/*
 * util_fd_get_type -- checks whether a file descriptor is associated
 *		       with a device dax or a normal file
 */
enum file_type
util_fd_get_type(int fd)
{
	LOG(3, "fd %d", fd);

#ifdef _WIN32
	return TYPE_NORMAL;
#else
	os_stat_t st;

	if (os_fstat(fd, &st) < 0) {
		ERR("!fstat");
		return OTHER_ERROR;
	}

	return util_stat_get_type(&st);
#endif
}

/*
 * util_file_get_type -- checks whether the path points to a device dax,
 *			 normal file or non-existent file
 */
enum file_type
util_file_get_type(const char *path)
{
	LOG(3, "path \"%s\"", path);

	if (path == NULL) {
		ERR("invalid (NULL) path");
		errno = EINVAL;
		return OTHER_ERROR;
	}

	int exists = util_file_exists(path);
	if (exists < 0)
		return OTHER_ERROR;

	if (!exists)
		return NOT_EXISTS;

#ifdef _WIN32
	return TYPE_NORMAL;
#else
	os_stat_t st;

	if (os_stat(path, &st) < 0) {
		ERR("!stat");
		return OTHER_ERROR;
	}

	return util_stat_get_type(&st);
#endif
}

/*
 * util_file_get_size -- returns size of a file
 */
ssize_t
util_file_get_size(const char *path)
{
	LOG(3, "path \"%s\"", path);

	int fd = os_open(path, O_RDONLY);
	if (fd < 0) {
		ERR("!open");
		return -1;
	}

	ssize_t size = util_fd_get_size(fd);
	(void) close(fd);

	return size;
}

/*
 * util_fd_get_size -- returns size of a file behind a given file descriptor
 */
ssize_t
util_fd_get_size(int fd)
{
	LOG(3, "fd %d", fd);

	struct pmem2_config cfg;
	size_t size;
	int ret;

	config_init(&cfg);
	ret = pmem2_config_set_fd(&cfg, fd);
	if (ret) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	ret = pmem2_config_get_file_size(&cfg, &size);
	if (ret) {
		errno = pmem2_err_to_errno(ret);
		return -1;
	}

	/* size is unsigned, this function returns signed */
	if (size >= INT64_MAX) {
		errno = ERANGE;
		ERR(
			"file size (%ld) too big to be represented in 64-bit signed integer",
			size);
		return -1;
	}

	LOG(4, "file length %zu", size);
	return (ssize_t)size;
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
	int flags = O_RDWR;
#ifdef _WIN32
	flags |= O_BINARY;
#endif

	if ((fd = os_open(path, flags)) < 0) {
		ERR("!open \"%s\"", path);
		return NULL;
	}

	ssize_t size = util_fd_get_size(fd);
	if (size < 0) {
		LOG(2, "cannot determine file length \"%s\"", path);
		goto out;
	}

	addr = util_map(fd, 0, (size_t)size, MAP_SHARED, 0, 0, NULL);
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
	int flags = O_RDWR;
#ifdef _WIN32
	flags |= O_BINARY;
#endif

	if ((fd = os_open(path, flags)) < 0) {
		ERR("!open \"%s\"", path);
		return -1;
	}

	ssize_t size = util_fd_get_size(fd);
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

	void *addr = util_map(fd, 0, (size_t)size, MAP_SHARED, 0, 0, NULL);
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

	enum file_type type = util_file_get_type(path);
	if (type < 0)
		return -1;

	if (type == TYPE_NORMAL) {
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

	enum file_type type = util_file_get_type(path);
	if (type < 0)
		return -1;

	if (type == TYPE_NORMAL) {
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

		ssize_t actual_size = util_fd_get_size(fd);
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

	enum file_type type = util_file_get_type(path);
	if (type < 0)
		return -1;

	if (type == TYPE_DEVDAX) {
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

/*
 * util_write_all -- a wrapper for util_write
 *
 * writes exactly count bytes from buf to file referred to by fd
 * returns -1 on error, 0 otherwise
 */
int
util_write_all(int fd, const char *buf, size_t count)
{
	ssize_t n_wrote = 0;
	size_t total = 0;

	while (count > total) {
		n_wrote = util_write(fd, buf, count - total);
		if (n_wrote <= 0)
			return -1;

		buf += (size_t)n_wrote;
		total += (size_t)n_wrote;
	}

	return 0;
}
