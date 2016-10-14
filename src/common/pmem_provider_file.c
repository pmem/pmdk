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
 * pmem_provider_file.c -- implementation of a regular file pmem provider
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

/*
 * provider_regular_file_type_match -- (internal) checks whether the pmem
 *	provider is of regular file type
 */
static int
provider_regular_file_type_match(struct pmem_provider *p)
{
	if (!p->exists) /* if it doesn't exist a regular file will be created */
		return 1;

	if (!S_ISCHR(p->st.st_mode))
		return 1;

	return 0;
}

/*
 * provider_regular_file_open -- (internal) opens, or creates, a regular file
 */
static int
provider_regular_file_open(struct pmem_provider *p,
	int flags, mode_t mode, int tmp)
{
#ifdef _WIN32
	/*
	 * POSIX does not differentiate between binary/text file modes and
	 * neither should we.
	 */
	flags |= O_BINARY;
	if (!mode)
		mode = S_IWRITE | S_IREAD;
#endif

	if (tmp) {
#if USE_O_TMPFILE
		flags |= O_TMPFILE;
		if ((p->fd = open(p->path, flags, mode)) < 0)
			return -1;
#else
		if ((p->fd = util_tmpfile(p->path, "/pmem.XXXXXX")) < 0)
			return -1;
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
 * provider_regular_file_close -- (internal) closes the pmem provider
 */
static void
provider_regular_file_close(struct pmem_provider *p)
{
	int olderrno = errno;
	(void) close(p->fd);
	errno = olderrno;
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
 * provider_regular_file_get_size --
 *	(internal) returns the size of a regular file
 */
static ssize_t
provider_regular_file_get_size(struct pmem_provider *p)
{
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
		if (ftruncate(p->fd, (off_t)size) != 0) {
			ERR("!ftruncate");
			return -1;
		}
	} else {
		int olderrno = errno;
		errno = posix_fallocate(p->fd, 0, (off_t)size);
		if (errno != 0) {
			ERR("!posix_fallocate");
			return -1;
		}
		errno = olderrno;
	}
	/* refresh stat, size might have changed */
	if (util_fstat(p->fd, &p->st) < 0) {
		return -1;
	}
	return 0;
}

/*
 * provider_regular_file_lock -- (internal) grabs a file lock, released on close
 */
static int
provider_regular_file_lock(struct pmem_provider *p)
{
	return flock(p->fd, LOCK_EX | LOCK_NB);
}


/*
 * provider_regular_file_always_pmem -- (internal) returns whether the provider
 *	always guarantees that the storage is persistent.
 *
 * For regular files persistence depends on the underlying file system.
 */
static int
provider_regular_file_always_pmem(void)
{
	return 0;
}

/*
 * provider_regular_file_map -- (internal) creates a new virtual address space
 *	mapping
 */
static void *
provider_regular_file_map(struct pmem_provider *p, size_t alignment)
{
	ssize_t size = p->pops->get_size(p);
	if (size < 0)
		return NULL;

	return util_map(p->fd, (size_t)size, 0, alignment);
}

/*
 * provider_regular_file_protect_range -- (internal) changes protection for the
 *	provided memory range
 */
static int
provider_regular_file_protect_range(struct pmem_provider *p,
	void *addr, size_t len, enum pmem_provider_protection prot)
{
	uintptr_t uptr;
	int retval;

	/*
	 * mprotect requires addr to be a multiple of pagesize, so
	 * adjust addr and len to represent the full 4k chunks
	 * covering the given range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uptr = (uintptr_t)addr & ~(Pagesize - 1);

	int protv = -1;
	switch (prot) {
		case PMEM_PROT_NONE:
			protv = PROT_NONE;
			break;
		case PMEM_PROT_READ_ONLY:
			protv = PROT_READ;
			break;
		case PMEM_PROT_READ_WRITE:
			protv = PROT_READ | PROT_WRITE;
			break;
		default:
			ASSERT(0);
			break;
	}
	ASSERTne(protv, -1);

	if ((retval = mprotect((void *)uptr, len, protv)) < 0)
		ERR("!mprotect");

	return retval;
}

static struct pmem_provider_ops pmem_provider_regular_file_ops = {
	.type_match = provider_regular_file_type_match,
	.open = provider_regular_file_open,
	.close = provider_regular_file_close,
	.unlink = provider_regular_file_unlink,
	.lock = provider_regular_file_lock,
	.map = provider_regular_file_map,
	.get_size = provider_regular_file_get_size,
	.allocate_space = provider_regular_file_allocate_space,
	.always_pmem = provider_regular_file_always_pmem,
	.protect_range = provider_regular_file_protect_range,
};

PMEM_PROVIDER_TYPE(PMEM_PROVIDER_REGULAR_FILE, &pmem_provider_regular_file_ops);
