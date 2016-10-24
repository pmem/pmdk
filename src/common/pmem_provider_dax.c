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
 * pmem_provider_dax.c -- implementation of a dax device pmem provider
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <sys/file.h>

#include "pmem_provider.h"
#include "mmap.h"
#include "out.h"

#define DEVICE_DAX_PREFIX "/sys/class/dax"
#define MAX_SIZE_LENGTH 64

/*
 * provider_device_dax_type_match -- (internal) checks whether the pmem provider
 *	is of device dax type
 */
static int
provider_device_dax_type_match(struct pmem_provider *p)
{
	char path[PATH_MAX];
	snprintf(path, PATH_MAX, "/sys/dev/char/%d:%d/subsystem",
		major(p->st.st_rdev), minor(p->st.st_rdev));

	char npath[PATH_MAX];
	char *rpath = realpath(path, npath);
	if (rpath == NULL)
		return PMEM_PROVIDER_UNKNOWN;

	return strcmp(DEVICE_DAX_PREFIX, rpath) == 0;
}

/*
 * provider_device_dax_open -- (internal) opens a dax device
 */
static int
provider_device_dax_open(struct pmem_provider *p,
	int flags, mode_t mode, int tmp)
{
#ifdef O_TMPFILE
	if (flags & O_TMPFILE)
		tmp = 1;
#endif

	if (flags & O_CREAT || tmp) {
		flags &= ~O_CREAT;
		flags &= ~O_EXCL; /* just in case */
	}

	if ((p->fd = open(p->path, flags, mode)) < 0)
		return -1;

	return 0;
}

/*
 * provider_device_dax_close -- (internal) closes the pmem provider
 */
static void
provider_device_dax_close(struct pmem_provider *p)
{
	int olderrno = errno;
	(void) close(p->fd);
	errno = olderrno;
}

/*
 * provider_device_dax_rm -- zeroes the device
 */
static int
provider_device_dax_rm(struct pmem_provider *p)
{
	int fd;
	if (p->fd == -1) {
		if ((fd = open(p->path, O_RDWR)) < -1)
			return -1;
	} else {
		fd = p->fd;
	}

	ssize_t size = p->pops->get_size(p);
	if (size < 0)
		return -1;

	void *addr = util_map(fd, (size_t)size, 0, 0);
	if (addr == NULL)
		return -1;

	/* zero initialize the entire device */
	memset(addr, 0, (size_t)size);

	util_unmap(addr, (size_t)size);

	if (p->fd == -1) {
		int olderrno = errno;
		close(fd);
		errno = olderrno;
	}

	return 0;
}

/*
 * provider_device_dax_map -- (internal) creates a new virtual
 *	address space mapping
 */
static void *
provider_device_dax_map(struct pmem_provider *p, size_t alignment)
{
	ssize_t size = p->pops->get_size(p);
	if (size < 0)
		return NULL;

	return util_map(p->fd, (size_t)size, 0, alignment);
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
 * provider_device_dax_lock -- (internal) grabs a file lock, released on close
 */
static int
provider_device_dax_lock(struct pmem_provider *p)
{
	return flock(p->fd, LOCK_EX | LOCK_NB);
}

/*
 * provider_device_dax_always_pmem -- (internal) returns whether the provider
 *	always guarantees that the storage is persistent.
 *
 * Always true for device dax.
 */
static int
provider_device_dax_always_pmem(void)
{
	return 1;
}

/*
 * provider_dax_protect_range -- (internal) changes protection for the
 *	provided memory range
 *
 * Due to the lack of transparent huge page support in dax device changing
 * protection with the desired granularity (4 kilobytes) is impossible.
 */
static int
provider_dax_protect_range(struct pmem_provider *p,
	void *addr, size_t len, enum pmem_provider_protection prot)
{
	return 0;
}

static struct pmem_provider_ops pmem_provider_device_dax_ops = {
	.type_match = provider_device_dax_type_match,
	.open = provider_device_dax_open,
	.close = provider_device_dax_close,
	.rm = provider_device_dax_rm,
	.lock = provider_device_dax_lock,
	.map = provider_device_dax_map,
	.get_size = provider_device_dax_get_size,
	.allocate_space = provider_device_dax_allocate_space,
	.always_pmem = provider_device_dax_always_pmem,
	.protect_range = provider_dax_protect_range,
};

PMEM_PROVIDER_TYPE(PMEM_PROVIDER_DEVICE_DAX, &pmem_provider_device_dax_ops);
