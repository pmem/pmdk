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
 * pmem_provider.c -- persistent memory provider interface and its two basic
 *	implementations: a regular file and dax character device.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "pmem_provider.h"
#include "mmap.h"
#include "out.h"

#ifndef USE_O_TMPFILE
#ifdef O_TMPFILE
#define USE_O_TMPFILE 1
#else
#define USE_O_TMPFILE 0
#endif
#endif

#define DEVICE_DAX_PREFIX "/sys/class/dax"
#define MAX_SIZE_LENGTH 64

enum pmem_provider_type {
	PMEM_PROVIDER_UNKNOWN,
	PMEM_PROVIDER_REGULAR_FILE,
	PMEM_PROVIDER_DEVICE_DAX,

	MAX_PMEM_PROVIDER_TYPE
};

/*
 * provider_device_dax_open -- (internal) opens a dax device
 */
static int
provider_device_dax_open(struct pmem_provider *p,
	int flags, mode_t mode, int tmp)
{
	if (tmp)
		return -1;

	if (flags & O_CREAT)
		return -1;

#ifdef O_TMPFILE
	if (flags & O_TMPFILE)
		return -1;
#endif

	if ((p->fd = open(p->path, flags, mode)) < 0)
		return -1;

	return 0;
}

/*
 * provider_regular_file_open -- (internal) opens, or creates, a regular file
 */
static int
provider_regular_file_open(struct pmem_provider *p,
	int flags, mode_t mode, int tmp)
{
	if (tmp) {
#if USE_O_TMPFILE
		open_flags |= O_TMPFILE;
		if ((p->fd = open(p->path, flags, mode)) < 0)
			return -1;
#else
		if ((p->fd = util_tmpfile(p->path, "/pmem.XXXXXX")) < 0) {
			return -1;
		}
#endif
	} else {
		if ((p->fd = open(p->path, flags, mode)) < 0)
			return -1;
	}

	if (!p->exists) {
		if (util_fstat(p->fd, &p->st) < 0) {
			p->pops->unlink(p);
			p->pops->close(p);
			return -1;
		}
		p->exists = 1;
	}

	return 0;
}

/*
 * provider_common_close -- (internal) closes the pmem provider
 */
static void
provider_common_close(struct pmem_provider *p)
{
	int olderrno = errno;
	(void) close(p->fd);
	errno = olderrno;
}

/*
 * provider_device_dax_unlink --
 *	(internal) no-op, doesn't make sense on dax device.
 */
static void
provider_device_dax_unlink(struct pmem_provider *p)
{
	ASSERT(0);
}

/*
 * provider_regular_file_unlink -- (internal) unlinks a regular file
 */
static void
provider_regular_file_unlink(struct pmem_provider *p)
{
	int olderrno = errno;
	(void) unlink(p->path);
	errno = olderrno;
}

/*
 * provider_common_map -- (internal) creates a new virtual address space mapping
 */
static void *
provider_common_map(struct pmem_provider *p, size_t alignment)
{
	ssize_t size = p->pops->get_size(p);
	if (size < 0)
		return NULL;

	return util_map(p->fd, (size_t)size, 0, alignment);
}

/*
 * pmem_provider_query_type -- (internal) checks the type of a pmem provider
 */
static enum pmem_provider_type
pmem_provider_query_type(struct pmem_provider *p)
{
	if (!p->exists) /* if it doesn't exist a regular file will be created */
		return PMEM_PROVIDER_REGULAR_FILE;

	if (!S_ISCHR(p->st.st_mode))
		return PMEM_PROVIDER_REGULAR_FILE;

	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "/sys/dev/char/%d:%d/subsystem",
		major(p->st.st_rdev), minor(p->st.st_rdev));

	char npath[PATH_MAX];
	char *rpath = realpath(path, npath);
	if (rpath == NULL)
		return PMEM_PROVIDER_UNKNOWN;

	return strncmp(DEVICE_DAX_PREFIX, rpath, strlen(DEVICE_DAX_PREFIX))
		== 0 ? PMEM_PROVIDER_DEVICE_DAX : PMEM_PROVIDER_UNKNOWN;
}

/*
 * provider_device_dax_get_size --
 *	(internal) returns the size of a dax char device
 */
static ssize_t
provider_device_dax_get_size(struct pmem_provider *p)
{
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "/sys/dev/char/%d:%d/size",
		major(p->st.st_rdev), minor(p->st.st_rdev));
	int fd = open(path, O_RDONLY);
	if (fd < 0)
		goto err;

	char sizebuf[MAX_SIZE_LENGTH] = {1};
	ssize_t nread;
	if ((nread = read(fd, sizebuf, MAX_SIZE_LENGTH)) < 0)
		goto err_read;

	sizebuf[nread] = 0; /* null termination */

	char *endptr;

	int olderrno = errno;
	errno = 0;

	ssize_t size = strtoll(sizebuf, &endptr, 0);
	if (endptr == sizebuf || *endptr != '\n' ||
		((size == LLONG_MAX || size == LLONG_MIN) && errno == ERANGE))
		goto err_read;

	errno = olderrno;

	(void) close(fd);

	return size;

err_read:
	(void) close(fd);
err:
	return -1;
}

/*
 * provider_regular_file_get_size --
 *	(internal) returns the size of a regular file
 */
static ssize_t
provider_regular_file_get_size(struct pmem_provider *p)
{
	/* refresh stat, size might have changed */
	if (util_fstat(p->fd, &p->st) < 0) {
		return -1;
	}

	if (p->st.st_size < 0)
		return -1;

	return (ssize_t)p->st.st_size;
}

/*
 * provider_regular_file_allocate_space --
 *	(internal) reserves space in the provider, either by allocating the
 *	blocks or truncating the file to requested size.
 */
static int
provider_regular_file_allocate_space(struct pmem_provider *p,
	size_t size, int sparse)
{
	if (sparse) {
		if (ftruncate(p->fd, (off_t)size) != 0)
			return -1;
	} else {
		int olderrno = errno;
		errno = posix_fallocate(p->fd, 0, (off_t)size);
		if (errno != 0) {
			return -1;
		}
		errno = olderrno;
	}
	return 0;
}

/*
 * provider_device_dax_allocate_space --
 *	(internal) device dax is fixed-length, this is a no-op
 */
static int
provider_device_dax_allocate_space(struct pmem_provider *p,
	size_t size, int sparse)
{
	return 0;
}

/*
 * provider_device_dax_always_pmem -- (internal) returns whether the provider
 *	always guarantees that the storage is persistent.
 *
 * Always true for device dax.
 */
static int
provider_device_dax_always_pmem()
{
	return 1;
}

/*
 * provider_regular_file_always_pmem -- (internal) returns whether the provider
 *	always guarantees that the storage is persistent.
 *
 * For regular files persistence depends on the underlying file system.
 */
static int
provider_regular_file_always_pmem()
{
	return 0;
}

static struct pmem_provider_ops
pmem_provider_operations[MAX_PMEM_PROVIDER_TYPE] = {
	[PMEM_PROVIDER_UNKNOWN] = {
		.open = NULL,
		.close = NULL,
		.unlink = NULL,
		.map = NULL,
		.get_size = NULL,
		.allocate_space = NULL,
		.always_pmem = NULL,
	},
	[PMEM_PROVIDER_REGULAR_FILE] = {
		.open = provider_regular_file_open,
		.close = provider_common_close,
		.unlink = provider_regular_file_unlink,
		.map = provider_common_map,
		.get_size = provider_regular_file_get_size,
		.allocate_space = provider_regular_file_allocate_space,
		.always_pmem = provider_regular_file_always_pmem,
	},
	[PMEM_PROVIDER_DEVICE_DAX] = {
		.open = provider_device_dax_open,
		.close = provider_common_close,
		.unlink = provider_device_dax_unlink,
		.map = provider_common_map,
		.get_size = provider_device_dax_get_size,
		.allocate_space = provider_device_dax_allocate_space,
		.always_pmem = provider_device_dax_always_pmem,
	},
};

/*
 * pmem_file_init -- initializes an instance of peristent memory provider
 */
int
pmem_provider_init(struct pmem_provider *p, const char *path)
{
	p->path = Strdup(path);
	if (p->path == NULL)
		goto error_path_strdup;

	p->exists = 1;
	int olderrno = errno;
	if (util_stat(path, &p->st) < 0) {
		if (errno == ENOENT)
			p->exists = 0;
		else
			goto error_init;
	}
	errno = olderrno; /* file not existing is not an error */

	enum pmem_provider_type type = pmem_provider_query_type(p);
	if (type == PMEM_PROVIDER_UNKNOWN)
		goto error_init;

	ASSERTne(type, MAX_PMEM_PROVIDER_TYPE);

	p->pops = &pmem_provider_operations[type];

	return 0;

error_init:
	Free(p->path);
error_path_strdup:
	return -1;
}

/*
 * pmem_provider_fini -- cleanups an instance of persistent memory provider
 */
void
pmem_provider_fini(struct pmem_provider *p)
{
	Free(p->path);
}
