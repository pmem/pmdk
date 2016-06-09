/*
 * Copyright 2014-2016, Intel Corporation
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

#include <errno.h>
#include <sys/file.h>
#include <unistd.h>

#include "file.h"
#include "out.h"

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
	int flags;
#ifndef _WIN32
	flags = 0;
#else
	flags = S_IWRITE | S_IREAD;
#endif

	/*
	 * Create file without any permission. It will be granted once
	 * initialization completes.
	 */
	if ((fd = open(path, O_RDWR | O_CREAT | O_EXCL, flags)) < 0) {
		ERR("!open %s", path);
		return -1;
	}

	if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
		ERR("!flock");
		goto err;
	}

	if ((errno = posix_fallocate(fd, 0, (off_t)size)) != 0) {
		ERR("!posix_fallocate");
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

		util_stat_t stbuf;
		if (util_fstat(fd, &stbuf) < 0) {
			ERR("!fstat %s", path);
			goto err;
		}
		if (stbuf.st_size < 0) {
			ERR("stat %s: negative size", path);
			errno = EINVAL;
			goto err;
		}
		if ((size_t)stbuf.st_size < minsize) {
			ERR("size %zu smaller than %zu",
					(size_t)stbuf.st_size, minsize);
			errno = EINVAL;
			goto err;
		}

		if (size)
			*size = (size_t)stbuf.st_size;
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
