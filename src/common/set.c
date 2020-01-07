// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * Copyright (c) 2016, Microsoft Corporation. All rights reserved.
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
 * set.c -- pool set utilities
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <endian.h>
#include <errno.h>
#include <stddef.h>
#include <time.h>
#include <ctype.h>
#include <linux/limits.h>
#include <sys/mman.h>

#include "libpmem.h"
#include "librpmem.h"
#include "set.h"
#include "file.h"
#include "os.h"
#include "mmap.h"
#include "util.h"
#include "out.h"
#include "dlsym.h"
#include "valgrind_internal.h"
#include "sys_util.h"
#include "util_pmem.h"
#include "fs.h"
#include "os_deep.h"
#include "badblock.h"

#define LIBRARY_REMOTE "librpmem.so.1"
#define SIZE_AUTODETECT_STR "AUTO"

#define PMEM_EXT ".pmem"
#define PMEM_EXT_LEN sizeof(PMEM_EXT)
#define PMEM_FILE_PADDING 6
#define PMEM_FILE_NAME_MAX_LEN 20
#define PMEM_FILE_MAX_LEN (PMEM_FILE_NAME_MAX_LEN + PMEM_FILE_PADDING)

static RPMEMpool *(*Rpmem_create)(const char *target, const char *pool_set_name,
			void *pool_addr, size_t pool_size, unsigned *nlanes,
			const struct rpmem_pool_attr *rpmem_attr);
static RPMEMpool *(*Rpmem_open)(const char *target, const char *pool_set_name,
			void *pool_addr, size_t pool_size, unsigned *nlanes,
			struct rpmem_pool_attr *rpmem_attr);
int (*Rpmem_close)(RPMEMpool *rpp);
int (*Rpmem_persist)(RPMEMpool *rpp, size_t offset, size_t length,
			unsigned lane, unsigned flags);
int (*Rpmem_deep_persist)(RPMEMpool *rpp, size_t offset, size_t length,
			unsigned lane);
int (*Rpmem_read)(RPMEMpool *rpp, void *buff, size_t offset,
		size_t length, unsigned lane);
int (*Rpmem_remove)(const char *target, const char *pool_set_name, int flags);
int (*Rpmem_set_attr)(RPMEMpool *rpp, const struct rpmem_pool_attr *rattr);

static int Remote_replication_available;
static os_mutex_t Remote_lock;
static void *Rpmem_handle_remote;

int Prefault_at_open = 0;
int Prefault_at_create = 0;
int SDS_at_create = POOL_FEAT_INCOMPAT_DEFAULT & POOL_E_FEAT_SDS ? 1 : 0;
int Fallocate_at_create = 1;
int COW_at_open = 0;

/* list of pool set option names and flags */
static const struct pool_set_option Options[] = {
	{ "SINGLEHDR", OPTION_SINGLEHDR },
#ifndef _WIN32
	{ "NOHDRS", OPTION_NOHDRS },
#endif
	{ NULL, OPTION_UNKNOWN }
};

/*
 * util_remote_init -- initialize remote replication
 */
void
util_remote_init(void)
{
	LOG(3, NULL);

	/* XXX Is duplicate initialization really okay? */
	if (!Remote_replication_available) {
		util_mutex_init(&Remote_lock);
		Remote_replication_available = 1;
	}
}

/*
 * util_remote_fini -- finalize remote replication
 */
void
util_remote_fini(void)
{
	LOG(3, NULL);

	util_remote_unload();

	/* XXX Okay to be here if not initialized? */
	if (Remote_replication_available) {
		Remote_replication_available = 0;
		util_mutex_destroy(&Remote_lock);
	}
}

/*
 * util_dl_check_error -- check libdl error
 */
static int
util_dl_check_error(void *handle, const char *func)
{
	LOG(15, "handle %p func %s", handle, func);

	if (handle == NULL) {
		char *errstr = util_dlerror();
		if (errstr)
			ERR("%s(): %s", func, errstr);
		errno = ELIBACC;
		return -1;
	}
	return 0;
}

/*
 * util_remote_unload_core -- (internal) unload remote library (core function)
 */
static void
util_remote_unload_core(void)
{
	if (Rpmem_handle_remote != NULL) {
		util_dlclose(Rpmem_handle_remote);
		Rpmem_handle_remote = NULL;
	}
	Rpmem_create = NULL;
	Rpmem_open = NULL;
	Rpmem_close = NULL;
	Rpmem_persist = NULL;
	Rpmem_deep_persist = NULL;
	Rpmem_read = NULL;
	Rpmem_remove = NULL;
	Rpmem_set_attr = NULL;
}

/*
 * util_remote_unload -- unload remote library
 */
void
util_remote_unload(void)
{
	LOG(3, NULL);

	if (!Remote_replication_available)
		return;

	util_mutex_lock(&Remote_lock);

	util_remote_unload_core();

	util_mutex_unlock(&Remote_lock);
}

/*
 * util_remote_load -- load remote library
 */
int
util_remote_load(void)
{
	LOG(3, NULL);

	if (!Remote_replication_available) {
		ERR("remote replication is not available");
		return -1;
	}

	CHECK_FUNC_COMPATIBLE(rpmem_create, *Rpmem_create);
	CHECK_FUNC_COMPATIBLE(rpmem_open, *Rpmem_open);
	CHECK_FUNC_COMPATIBLE(rpmem_close, *Rpmem_close);
	CHECK_FUNC_COMPATIBLE(rpmem_persist, *Rpmem_persist);
	CHECK_FUNC_COMPATIBLE(rpmem_deep_persist, *Rpmem_deep_persist);
	CHECK_FUNC_COMPATIBLE(rpmem_read, *Rpmem_read);
	CHECK_FUNC_COMPATIBLE(rpmem_remove, *Rpmem_remove);

	util_mutex_lock(&Remote_lock);

	if (Rpmem_handle_remote)
		goto end;

	Rpmem_handle_remote = util_dlopen(LIBRARY_REMOTE);
	if (util_dl_check_error(Rpmem_handle_remote, "dlopen")) {
		ERR("the pool set requires a remote replica, "
		    "but the '%s' library cannot be loaded",
		    LIBRARY_REMOTE);
		goto err;
	}

	Rpmem_create = util_dlsym(Rpmem_handle_remote, "rpmem_create");
	if (util_dl_check_error(Rpmem_create, "dlsym")) {
		ERR("symbol 'rpmem_create' not found");
		goto err;
	}

	Rpmem_open = util_dlsym(Rpmem_handle_remote, "rpmem_open");
	if (util_dl_check_error(Rpmem_open, "dlsym")) {
		ERR("symbol 'rpmem_open' not found");
		goto err;
	}

	Rpmem_close = util_dlsym(Rpmem_handle_remote, "rpmem_close");
	if (util_dl_check_error(Rpmem_close, "dlsym")) {
		ERR("symbol 'rpmem_close' not found");
		goto err;
	}

	Rpmem_persist = util_dlsym(Rpmem_handle_remote, "rpmem_persist");
	if (util_dl_check_error(Rpmem_persist, "dlsym")) {
		ERR("symbol 'rpmem_persist' not found");
		goto err;
	}

	Rpmem_deep_persist = util_dlsym(Rpmem_handle_remote,
			"rpmem_deep_persist");
	if (util_dl_check_error(Rpmem_deep_persist, "dlsym")) {
		ERR("symbol 'rpmem_deep_persist' not found");
		goto err;
	}

	Rpmem_read = util_dlsym(Rpmem_handle_remote, "rpmem_read");
	if (util_dl_check_error(Rpmem_read, "dlsym")) {
		ERR("symbol 'rpmem_read' not found");
		goto err;
	}

	Rpmem_remove = util_dlsym(Rpmem_handle_remote, "rpmem_remove");
	if (util_dl_check_error(Rpmem_remove, "dlsym")) {
		ERR("symbol 'rpmem_remove' not found");
		goto err;
	}

	Rpmem_set_attr = util_dlsym(Rpmem_handle_remote, "rpmem_set_attr");
	if (util_dl_check_error(Rpmem_set_attr, "dlsym")) {
		ERR("symbol 'rpmem_set_attr' not found");
		goto err;
	}

end:
	util_mutex_unlock(&Remote_lock);
	return 0;

err:
	LOG(4, "error clean up");
	util_remote_unload_core();
	util_mutex_unlock(&Remote_lock);
	return -1;
}

/* reserve space for size, path and some whitespace and/or comment */

enum parser_codes {
	PARSER_CONTINUE = 0,
	PARSER_PMEMPOOLSET,
	PARSER_REPLICA,
	PARSER_INVALID_TOKEN,
	PARSER_REMOTE_REPLICA_EXPECTED,
	PARSER_WRONG_SIZE,
	PARSER_CANNOT_READ_SIZE,
	PARSER_ABSOLUTE_PATH_EXPECTED,
	PARSER_RELATIVE_PATH_EXPECTED,
	PARSER_SET_NO_PARTS,
	PARSER_REP_NO_PARTS,
	PARSER_REMOTE_REP_UNEXPECTED_PARTS,
	PARSER_SIZE_MISMATCH,
	PARSER_OUT_OF_MEMORY,
	PARSER_OPTION_UNKNOWN,
	PARSER_OPTION_EXPECTED,
	PARSER_FORMAT_OK,
	PARSER_MAX_CODE
};

static const char *parser_errstr[PARSER_MAX_CODE] = {
	"", /* parsing */
	"the first line must be exactly 'PMEMPOOLSET'",
	"exactly 'REPLICA' expected",
	"invalid token found in the current line",
	"address of remote node and descriptor of remote pool set expected",
	"incorrect format of size",
	"cannot determine size of a part",
	"incorrect path (must be an absolute one)",
	"incorrect descriptor (must be a relative path)",
	"no pool set parts",
	"no replica parts",
	"unexpected parts for remote replica",
	"sizes of pool set and replica mismatch",
	"allocating memory failed",
	"unknown option",
	"missing option name",
	"" /* format correct */
};

/*
 * util_replica_force_page_allocation - (internal) forces page allocation for
 * replica
 */
static void
util_replica_force_page_allocation(struct pool_replica *rep)
{
	volatile char *cur_addr = rep->part[0].addr;
	char *addr_end = (char *)cur_addr + rep->resvsize;
	for (; cur_addr < addr_end; cur_addr += Pagesize) {
		*cur_addr = *cur_addr;
		VALGRIND_SET_CLEAN(cur_addr, 1);
	}
}

/*
 * util_map_hdr -- map a header of a pool set
 */
int
util_map_hdr(struct pool_set_part *part, int flags, int rdonly)
{
	LOG(3, "part %p flags %d", part, flags);

	COMPILE_ERROR_ON(POOL_HDR_SIZE == 0);
	ASSERTeq(POOL_HDR_SIZE % Pagesize, 0);

	/*
	 * Workaround for Device DAX not allowing to map a portion
	 * of the device if offset/length are not aligned to the internal
	 * device alignment (page size).  I.e. if the device alignment
	 * is 2M, we cannot map the 4K header, but need to align the mapping
	 * length to 2M.
	 *
	 * According to mmap(2), system should automatically align mapping
	 * length to be a multiple of the underlying page size, but it's
	 * not true for Device DAX.
	 */
	size_t hdrsize = part->alignment > POOL_HDR_SIZE
			? part->alignment : POOL_HDR_SIZE;

	void *addr = NULL;

#if VG_MEMCHECK_ENABLED
	if (On_valgrind) {
		/* this is required only for Device DAX & memcheck */
		addr = util_map_hint(hdrsize, hdrsize);
		if (addr == MAP_FAILED) {
			LOG(1, "cannot find a contiguous region of given size");
			/* there's nothing we can do */
			return -1;
		}
	}
#endif

	int prot = rdonly ? PROT_READ : PROT_READ|PROT_WRITE;
	void *hdrp = util_map_sync(addr, hdrsize, prot, flags,
			part->fd, 0, &part->hdr_map_sync);
	if (hdrp == MAP_FAILED) {
		ERR("!mmap: %s", part->path);
		return -1;
	}

	part->hdrsize = hdrsize;
	part->hdr = hdrp;

	VALGRIND_REGISTER_PMEM_MAPPING(part->hdr, part->hdrsize);
	VALGRIND_REGISTER_PMEM_FILE(part->fd, part->hdr, part->hdrsize, 0);

	return 0;
}

/*
 * util_unmap_hdr -- unmap pool set part header
 */
void
util_unmap_hdr(struct pool_set_part *part)
{
	if (part->hdr == NULL || part->hdrsize == 0)
		return;

	LOG(4, "munmap: addr %p size %zu", part->hdr, part->hdrsize);
	VALGRIND_REMOVE_PMEM_MAPPING(part->hdr, part->hdrsize);
	if (munmap(part->hdr, part->hdrsize) != 0)
		/* this means there's a bug on the caller side */
		FATAL("!munmap: %s", part->path);
	part->hdr = NULL;
	part->hdrsize = 0;
}

/*
 * util_map_part -- map a part of a pool set
 */
int
util_map_part(struct pool_set_part *part, void *addr, size_t size,
	size_t offset, int flags, int rdonly)
{
	LOG(3, "part %p addr %p size %zu offset %zu flags %d",
		part, addr, size, offset, flags);

	ASSERTeq((uintptr_t)addr % Mmap_align, 0);
	ASSERTeq(offset % Mmap_align, 0);
	ASSERTeq(size % Mmap_align, 0);
	ASSERT(((os_off_t)offset) >= 0);
	ASSERTeq(offset % part->alignment, 0);
	ASSERT(offset < part->filesize);

	if (!size)
		size = (part->filesize - offset) & ~(part->alignment - 1);
	else
		size = roundup(size, part->alignment);

	int prot = rdonly ? PROT_READ : PROT_READ | PROT_WRITE;
	void *addrp = util_map_sync(addr, size, prot, flags, part->fd,
			(os_off_t)offset, &part->map_sync);
	if (addrp == MAP_FAILED) {
		ERR("!mmap: %s", part->path);
		return -1;
	}

	if (addr != NULL && (flags & MAP_FIXED) && addrp != addr) {
		ERR("unable to map at requested address %p", addr);
		munmap(addrp, size);
		return -1;
	}

	part->addr = addrp;
	part->size = size;

	VALGRIND_REGISTER_PMEM_MAPPING(part->addr, part->size);
	VALGRIND_REGISTER_PMEM_FILE(part->fd, part->addr, part->size, offset);

	return 0;
}

/*
 * util_unmap_part -- unmap a part of a pool set
 */
int
util_unmap_part(struct pool_set_part *part)
{
	LOG(3, "part %p", part);

	if (part->addr != NULL && part->size != 0) {
		LOG(4, "munmap: addr %p size %zu", part->addr, part->size);
		VALGRIND_REMOVE_PMEM_MAPPING(part->addr, part->size);
		if (munmap(part->addr, part->size) != 0) {
			ERR("!munmap: %s", part->path);
		}

		part->addr = NULL;
		part->size = 0;
	}

	return 0;
}

/*
 * util_unmap_parts -- unmap parts from start_index to the end_index
 */
int
util_unmap_parts(struct pool_replica *rep, unsigned start_index,
	unsigned end_index)
{
	LOG(3, "rep: %p, start_index: %u, end_index: %u", rep, start_index,
		end_index);

	for (unsigned p = start_index; p <= end_index; p++)
		util_unmap_part(&rep->part[p]);

	return 0;
}

/*
 * util_poolset_free -- free pool set info
 */
void
util_poolset_free(struct pool_set *set)
{
	LOG(3, "set %p", set);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote == NULL) {
			/* only local replicas have paths */
			for (unsigned p = 0; p < rep->nallocated; p++) {
				Free((void *)(rep->part[p].path));
			}
		} else {
			/* remote replica */
			ASSERTeq(rep->nparts, 1);
			Free(rep->remote->node_addr);
			Free(rep->remote->pool_desc);
			Free(rep->remote);
		}
		struct pool_set_directory *d;
		VEC_FOREACH_BY_PTR(d, &rep->directory) {
			Free((void *)d->path);
		}
		VEC_DELETE(&rep->directory);
		Free(set->replica[r]);
	}
	Free(set->path);
	Free(set);
}

/*
 * util_poolset_open -- open all replicas from a poolset
 */
int
util_poolset_open(struct pool_set *set)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (util_replica_open(set, r, MAP_SHARED)) {
			LOG(2, "replica open failed: replica %u", r);
			errno = EINVAL;
			return -1;
		}
	}

	return 0;
}

/*
 * util_replica_close_local -- close local replica, optionally delete the
 *                             replica's parts
 */
int
util_replica_close_local(struct pool_replica *rep, unsigned repn,
		enum del_parts_mode del)
{
	for (unsigned p = 0; p < rep->nparts; p++) {
		if (rep->part[p].fd != -1)
			(void) os_close(rep->part[p].fd);

		if ((del == DELETE_CREATED_PARTS && rep->part[p].created) ||
				del == DELETE_ALL_PARTS) {
			LOG(4, "unlink %s", rep->part[p].path);
			int olderrno = errno;
			if (util_unlink(rep->part[p].path) && errno != ENOENT) {
				ERR("!unlink %s failed (part %u, replica %u)",
						rep->part[p].path, p, repn);
				return -1;
			}
			errno = olderrno;
		}
	}
	return 0;
}

/*
 * util_replica_close_remote -- close remote replica, optionally delete the
 *                              replica
 */
int
util_replica_close_remote(struct pool_replica *rep, unsigned repn,
		enum del_parts_mode del)
{
	if (!rep->remote)
		return 0;

	if (rep->remote->rpp) {
		LOG(4, "closing remote replica #%u", repn);
		Rpmem_close(rep->remote->rpp);
		rep->remote->rpp = NULL;
	}

	if ((del == DELETE_CREATED_PARTS && rep->part[0].created) ||
			del == DELETE_ALL_PARTS) {
		LOG(4, "removing remote replica #%u", repn);
		int ret = Rpmem_remove(rep->remote->node_addr,
			rep->remote->pool_desc, 0);
		if (ret) {
			LOG(1, "!removing remote replica #%u failed", repn);
			return -1;
		}
	}
	return 0;
}

/*
 * util_poolset_close -- unmap and close all the parts of the pool set,
 *                       optionally delete parts
 */
void
util_poolset_close(struct pool_set *set, enum del_parts_mode del)
{
	LOG(3, "set %p del %d", set, del);

	int oerrno = errno;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		util_replica_close(set, r);

		struct pool_replica *rep = set->replica[r];
		if (!rep->remote)
			(void) util_replica_close_local(rep, r, del);
		else
			(void) util_replica_close_remote(rep, r, del);
	}

	/*
	 * XXX On FreeBSD, mmap()ing a file does not increment the flock()
	 *     reference count, so we had to keep the files open until now.
	 */
#ifdef __FreeBSD__
	util_poolset_fdclose_always(set);
#endif
	util_poolset_free(set);

	errno = oerrno;
}

/*
 * util_poolset_chmod -- change mode for all created files related to pool set
 */
int
util_poolset_chmod(struct pool_set *set, mode_t mode)
{
	LOG(3, "set %p mode %o", set, mode);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];

		/* skip remote replicas */
		if (rep->remote != NULL)
			continue;

		for (unsigned p = 0; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];

			/* skip not created or closed parts */
			if (!part->created || part->fd == -1)
				continue;

			os_stat_t stbuf;
			if (os_fstat(part->fd, &stbuf) != 0) {
				ERR("!fstat %d %s", part->fd, part->path);
				return -1;
			}

			if (stbuf.st_mode & ~(unsigned)S_IFMT) {
				LOG(1, "file permissions changed during pool "
					"initialization, file: %s (%o)",
					part->path,
					stbuf.st_mode & ~(unsigned)S_IFMT);
			}

			if (os_chmod(part->path, mode)) {
				ERR("!chmod %u/%u/%s", r, p, part->path);
				return -1;
			}
		}
	}

	return 0;
}

/*
 * util_poolset_fdclose_always -- close file descriptors related to pool set
 */
void
util_poolset_fdclose_always(struct pool_set *set)
{
	LOG(3, "set %p", set);

	for (unsigned r = 0; r < set->nreplicas; r++)
		util_replica_fdclose(set->replica[r]);
}

/*
 * util_poolset_fdclose -- close pool set file descriptors if not FreeBSD
 *
 * XXX On FreeBSD, mmap()ing a file does not increment the flock()
 *	reference count, so we need to keep the files open.
 */
void
util_poolset_fdclose(struct pool_set *set)
{
#ifdef __FreeBSD__
	LOG(3, "set %p: holding open", set);
#else
	util_poolset_fdclose_always(set);
#endif
}

/*
 * util_autodetect_size -- (internal) retrieves size of an existing file
 */
static ssize_t
util_autodetect_size(const char *path)
{
	enum file_type type = util_file_get_type(path);
	if (type < 0)
		return -1;

	if (type == TYPE_NORMAL) {
		ERR("size autodetection is supported only for device dax");
		return -1;
	}

	return util_file_get_size(path);
}

/*
 * parser_read_line -- (internal) read line and validate size and path
 *                      from a pool set file
 */
static enum parser_codes
parser_read_line(char *line, size_t *size, char **path)
{
	int ret;
	char *size_str;
	char *path_str;
	char *rest_str;
	char *saveptr = NULL; /* must be NULL initialized on Windows */

	size_str = strtok_r(line, " \t", &saveptr);
	path_str = strtok_r(NULL, " \t", &saveptr);
	rest_str = strtok_r(NULL, " \t", &saveptr);

	if (!size_str || !path_str || rest_str)
		return PARSER_INVALID_TOKEN;

	LOG(10, "size '%s' path '%s'", size_str, path_str);

	/*
	 * A format of the size is checked in detail. As regards the path,
	 * it is checked only if the read path is an absolute path.
	 * The rest should be checked during creating/opening the file.
	 */

	/* check if the read path is an absolute path */
	if (!util_is_absolute_path(path_str))
		return PARSER_ABSOLUTE_PATH_EXPECTED;

	*path = Strdup(path_str);
	if (!(*path)) {
		ERR("!Strdup");
		return PARSER_OUT_OF_MEMORY;
	}

	if (strcmp(SIZE_AUTODETECT_STR, size_str) == 0) {
		/*
		 * XXX: this should be done after the parsing completes, but
		 * currently this operation is performed in simply too many
		 * places in the code to move this someplace else.
		 */
		ssize_t s = util_autodetect_size(path_str);
		if (s < 0) {
			Free(*path);
			*path = NULL;
			return PARSER_CANNOT_READ_SIZE;
		}

		*size = (size_t)s;

		return PARSER_CONTINUE;
	}

	ret = util_parse_size(size_str, size);
	if (ret != 0 || *size == 0) {
		Free(*path);
		*path = NULL;
		return PARSER_WRONG_SIZE;
	}

	return PARSER_CONTINUE;
}

/*
 * parser_read_replica -- (internal) read line and validate remote replica
 *                        from a pool set file
 */
static enum parser_codes
parser_read_replica(char *line, char **node_addr, char **pool_desc)
{
	char *addr_str;
	char *desc_str;
	char *rest_str;
	char *saveptr = NULL; /* must be NULL initialized on Windows */

	addr_str = strtok_r(line, " \t", &saveptr);
	desc_str = strtok_r(NULL, " \t", &saveptr);
	rest_str = strtok_r(NULL, " \t", &saveptr);

	if (!addr_str || !desc_str)
		return PARSER_REMOTE_REPLICA_EXPECTED;

	if (rest_str)
		return PARSER_INVALID_TOKEN;

	LOG(10, "node address '%s' pool set descriptor '%s'",
		addr_str, desc_str);

	/* check if the descriptor is a relative path */
	if (util_is_absolute_path(desc_str))
		return PARSER_RELATIVE_PATH_EXPECTED;

	*node_addr = Strdup(addr_str);
	*pool_desc = Strdup(desc_str);

	if (!(*node_addr) || !(*pool_desc)) {
		ERR("!Strdup");
		if (*node_addr)
			Free(*node_addr);
		if (*pool_desc)
			Free(*pool_desc);
		return PARSER_OUT_OF_MEMORY;
	}

	return PARSER_CONTINUE;
}

/*
 * parser_read_options -- (internal) read line and validate options
 */
static enum parser_codes
parser_read_options(char *line, unsigned *options)
{
	LOG(3, "line '%s'", line);

	int opt_cnt = 0;
	char *saveptr = NULL; /* must be NULL initialized on Windows */

	char *opt_str = strtok_r(line, " \t", &saveptr);
	while (opt_str != NULL) {
		LOG(4, "option '%s'", opt_str);

		int i = 0;
		while (Options[i].name && strcmp(opt_str, Options[i].name) != 0)
			i++;

		if (Options[i].name == NULL) {
			LOG(4, "unknown option '%s'", opt_str);
			return PARSER_OPTION_UNKNOWN;
		}

		if (*options & Options[i].flag)
			LOG(4, "duplicated option '%s'", opt_str);

		*options |= Options[i].flag;

		opt_cnt++;
		opt_str = strtok_r(NULL, " \t", &saveptr);
	}

	if (opt_cnt == 0)
		return PARSER_OPTION_EXPECTED;

	return PARSER_CONTINUE;
}

/*
 * util_replica_reserve -- reserves part slots capacity in a replica
 */
static int
util_replica_reserve(struct pool_replica **repp, unsigned n)
{
	LOG(3, "replica %p n %u", *repp, n);

	struct pool_replica *rep = *repp;
	if (rep->nallocated >= n)
		return 0;

	rep = Realloc(rep, sizeof(struct pool_replica) +
		(n) * sizeof(struct pool_set_part));
	if (rep == NULL) {
		ERR("!Realloc");
		return -1;
	}

	size_t nsize = sizeof(struct pool_set_part) * (n - rep->nallocated);
	memset(rep->part + rep->nallocated, 0, nsize);

	rep->nallocated = n;
	*repp = rep;

	return 0;
}

/*
 * util_replica_add_part_by_idx -- (internal) allocates, initializes and adds a
 *	part structure at the provided location in the replica info
 */
static int
util_replica_add_part_by_idx(struct pool_replica **repp,
	const char *path, size_t filesize, unsigned p)
{
	LOG(3, "replica %p path %s filesize %zu", *repp, path, filesize);

	if (util_replica_reserve(repp, p + 1) != 0)
		return -1;

	struct pool_replica *rep = *repp;
	ASSERTne(rep, NULL);

	int is_dev_dax = 0;

	if (path != NULL) {
		enum file_type type = util_file_get_type(path);
		if (type == OTHER_ERROR)
			return -1;

		is_dev_dax = type == TYPE_DEVDAX;
	}

	rep->part[p].path = path;
	rep->part[p].filesize = filesize;
	rep->part[p].fd = -1;
	rep->part[p].is_dev_dax = is_dev_dax;
	rep->part[p].created = 0;
	rep->part[p].hdr = NULL;
	rep->part[p].addr = NULL;
	rep->part[p].remote_hdr = NULL;
	rep->part[p].has_bad_blocks = 0;

	if (is_dev_dax)
		rep->part[p].alignment = util_file_device_dax_alignment(path);
	else
		rep->part[p].alignment = Mmap_align;

	ASSERTne(rep->part[p].alignment, 0);

	rep->nparts += 1;

	return 0;
}

/*
 * util_replica_add_part -- adds a next part in replica info
 */
static int
util_replica_add_part(struct pool_replica **repp,
	const char *path, size_t filesize)
{
	LOG(3, "replica %p path \"%s\" filesize %zu", *repp, path, filesize);

	return util_replica_add_part_by_idx(repp, path,
		filesize, (*repp)->nparts);
}

/*
 * util_parse_add_part -- (internal) add a new part file to the replica info
 */
static int
util_parse_add_part(struct pool_set *set, const char *path, size_t filesize)
{
	LOG(3, "set %p path %s filesize %zu", set, path, filesize);

	ASSERTne(set, NULL);

	if (set->directory_based) {
		ERR("cannot mix directories and files in a set");
		errno = EINVAL;
		return -1;
	}

	return util_replica_add_part(&set->replica[set->nreplicas - 1],
		path, filesize);
}

/*
 * util_parse_add_directory --
 *	(internal) add a new directory to the replica info
 */
static int
util_parse_add_directory(struct pool_set *set, const char *path,
	size_t filesize)
{
	LOG(3, "set %p path %s filesize %zu", set, path, filesize);

	ASSERTne(set, NULL);

	struct pool_replica *rep = set->replica[set->nreplicas - 1];
	ASSERTne(rep, NULL);

	if (set->directory_based == 0) {
		if (rep->nparts > 0 || set->nreplicas > 1) {
			ERR("cannot mix directories and files in a set");
			errno = EINVAL;
			return -1;
		}
		set->directory_based = 1;
	}

	char *rpath = util_part_realpath(path);
	if (rpath == NULL) {
		ERR("cannot resolve realpath of new directory");
		return -1;
	}

	for (unsigned i = 0; i < set->nreplicas; ++i) {
		struct pool_replica *r = set->replica[i];
		struct pool_set_directory *dir;
		char *dpath = NULL;
		VEC_FOREACH_BY_PTR(dir, &r->directory) {
			dpath = util_part_realpath(dir->path);
			ASSERTne(dpath, NULL); /* must have been resolved */
			if (strcmp(rpath, dpath) == 0) {
				ERR("cannot use the same directory twice");
				errno = EEXIST;
				free(dpath);
				free(rpath);
				return -1;
			}
			free(dpath);
		}
	}
	free(rpath);

	struct pool_set_directory d;
	d.path = path;
	d.resvsize = filesize;

	if (VEC_PUSH_BACK(&rep->directory, d) != 0)
		return -1;

	rep->resvsize += filesize;

	return 0;
}

/*
 * util_parse_add_element --
 *	(internal) add a new element to the replica info
 */
static int
util_parse_add_element(struct pool_set *set, const char *path, size_t filesize)
{
	LOG(3, "set %p path %s filesize %zu", set, path, filesize);

	os_stat_t stat;

	int olderrno = errno;

	if (os_stat(path, &stat) == 0 && S_ISDIR(stat.st_mode))
		return util_parse_add_directory(set, path, filesize);

	errno = olderrno;

	return util_parse_add_part(set, path, filesize);
}

/*
 * util_parse_add_replica -- (internal) add a new replica to the pool set info
 */
static int
util_parse_add_replica(struct pool_set **setp)
{
	LOG(3, "setp %p", setp);

	ASSERTne(setp, NULL);

	struct pool_set *set = *setp;
	ASSERTne(set, NULL);

	set = Realloc(set, sizeof(struct pool_set) +
			(set->nreplicas + 1) * sizeof(struct pool_replica *));
	if (set == NULL) {
		ERR("!Realloc");
		return -1;
	}
	*setp = set;

	struct pool_replica *rep;
	rep = Zalloc(sizeof(struct pool_replica));
	if (rep == NULL) {
		ERR("!Zalloc");
		return -1;
	}

	VEC_INIT(&rep->directory);

	unsigned r = set->nreplicas++;

	set->replica[r] = rep;

	return 0;
}

/*
 * util_replica_check_map_sync -- (internal) check MAP_SYNC restrictions
 */
static int
util_replica_check_map_sync(struct pool_set *set, unsigned repidx,
	int check_hdr)
{
	LOG(3, "set %p repidx %u", set, repidx);

	struct pool_replica *rep = set->replica[repidx];

	int map_sync = rep->part[0].map_sync;

	for (unsigned p = 1; p < rep->nparts; p++) {
		if (map_sync != rep->part[p].map_sync) {
			ERR("replica #%u part %u %smapped with MAP_SYNC",
				repidx, p, rep->part[p].map_sync ? "" : "not");
			return -1;
		}
	}

	if (check_hdr) {
		for (unsigned p = 0; p < rep->nhdrs; p++) {
			if (map_sync != rep->part[p].hdr_map_sync) {
				ERR("replica #%u part %u header %smapped "
					"with MAP_SYNC", repidx, p,
					rep->part[p].hdr_map_sync ?
					"" : "not");
				return -1;
			}
		}
	}

	return 0;
}

/*
 * util_poolset_check_devdax -- (internal) check Device DAX restrictions
 */
static int
util_poolset_check_devdax(struct pool_set *set)
{
	LOG(3, "set %p", set);

	if (set->directory_based)
		return 0;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		int is_dev_dax = rep->part[0].is_dev_dax;

		for (unsigned p = 0; p < rep->nparts; p++) {
			if (rep->part[p].is_dev_dax != is_dev_dax) {
				ERR(
					"either all the parts must be Device DAX or none");
				return -1;
			}

			if (is_dev_dax && rep->nparts > 1 &&
					(set->options & (OPTION_SINGLEHDR |
					OPTION_NOHDRS)) == 0 &&
			    util_file_device_dax_alignment(rep->part[p].path)
					!= Pagesize) {
				ERR(
					"Multiple DAX devices with alignment other than 4KB. Use the SINGLEHDR poolset option.");
				return -1;
			}
		}
	}
	return 0;
}

/*
 * util_poolset_check_options -- (internal) check if poolset options are
 *                               admissible
 */
static int
util_poolset_check_options(struct pool_set *set)
{
	LOG(3, "set %p", set);
	if ((set->options & OPTION_SINGLEHDR) &&
			(set->options & OPTION_NOHDRS)) {
		ERR(
		"both SINGLEHDR and NOHDR poolset options used at the same time");
		return -1;
	}
	return 0;
}

/*
 * util_poolset_set_size -- (internal) calculate pool size
 */
static void
util_poolset_set_size(struct pool_set *set)
{
	LOG(3, "set %p", set);

	set->poolsize = SIZE_MAX;
	set->resvsize = SIZE_MAX;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];

		if (set->options & OPTION_SINGLEHDR)
			rep->nhdrs = 1;
		else if (set->options & OPTION_NOHDRS)
			rep->nhdrs = 0;
		else
			rep->nhdrs = rep->nparts;

		rep->repsize = 0;
		for (unsigned p = 0; p < rep->nparts; p++) {
			rep->repsize +=
				(rep->part[p].filesize & ~(Mmap_align - 1));
		}
		if (rep->nhdrs > 0)
			rep->repsize -= (rep->nhdrs - 1) * Mmap_align;

		if (rep->resvsize == 0)
			rep->resvsize = rep->repsize;

		/*
		 * Calculate pool size - choose the smallest replica size.
		 * Ignore remote replicas.
		 */
		if (rep->remote == NULL && rep->repsize < set->poolsize)
			set->poolsize = rep->repsize;
		if (rep->remote == NULL && rep->resvsize < set->resvsize)
			set->resvsize = rep->resvsize;
	}

	LOG(3, "pool size set to %zu", set->poolsize);
}

/*
 * util_parse_add_remote_replica -- (internal) add a new remote replica
 *                                  to the pool set info
 */
static int
util_parse_add_remote_replica(struct pool_set **setp, char *node_addr,
				char *pool_desc)
{
	LOG(3, "setp %p node_addr %s pool_desc %s", setp, node_addr, pool_desc);

	ASSERTne(setp, NULL);
	ASSERTne(node_addr, NULL);
	ASSERTne(pool_desc, NULL);

	int ret = util_parse_add_replica(setp);
	if (ret != 0)
		return ret;

	/*
	 * A remote replica has one fake part of size equal twice pool header
	 * size for storing pool header and pool descriptor.
	 */
	ret = util_parse_add_part(*setp, NULL, 2 * POOL_HDR_SIZE);
	if (ret != 0)
		return ret;

	struct pool_set *set = *setp;
	struct pool_replica *rep = set->replica[set->nreplicas - 1];
	ASSERTne(rep, NULL);

	rep->remote = Zalloc(sizeof(struct remote_replica));
	if (rep->remote == NULL) {
		ERR("!Malloc");
		return -1;
	}
	rep->remote->node_addr = node_addr;
	rep->remote->pool_desc = pool_desc;
	set->remote = 1;

	return 0;
}

/*
 * util_part_idx_by_file_name -- (internal) retrieves the part index from a
 *	name of the file that is an element of a directory poolset
 */
static long
util_part_idx_by_file_name(const char *filename)
{
	LOG(3, "filename \"%s\"", filename);

	int olderrno = errno;
	errno = 0;
	long part_idx = strtol(filename, NULL, 10);
	if (errno != 0)
		return -1;

	errno = olderrno;

	return part_idx;
}

/*
 * util_poolset_directory_load -- (internal) loads and initializes all
 *	existing parts in a single directory
 */
static int
util_poolset_directory_load(struct pool_replica **repp, const char *directory)
{
	LOG(3, "rep %p dir \"%s\"", *repp, directory);

	struct fs *f = fs_new(directory);
	if (f == NULL) {
		ERR("!fs_new: \"%s\"", directory);
		return -1;
	}

	int nparts = 0;
	char *path = NULL;

	struct fs_entry *entry;
	while ((entry = fs_read(f)) != NULL) {
		if (entry->level != 1)
			continue;
		if (entry->type != FS_ENTRY_FILE)
			continue;
		if (entry->namelen < PMEM_EXT_LEN)
			continue;
		const char *ext = entry->path + entry->pathlen -
			PMEM_EXT_LEN + 1;
		if (strcmp(PMEM_EXT, ext) != 0)
			continue;

		long part_idx = util_part_idx_by_file_name(entry->name);
		if (part_idx < 0)
			continue;

		ssize_t size = util_file_get_size(entry->path);
		if (size < 0) {
			LOG(2,
			"cannot read size of file (%s) in a poolset directory",
			entry->path);
			goto err;
		}

		if ((path = Strdup(entry->path)) == NULL) {
			ERR("!Strdup");
			goto err;
		}

		if (util_replica_add_part_by_idx(repp, path,
				(size_t)size, (unsigned)part_idx) != 0) {
			ERR("unable to load part %s", entry->path);
			goto err;
		}
		nparts++;
	}

	fs_delete(f);
	return nparts;

err:
	fs_delete(f);
	return -1;
}

/*
 * util_poolset_directories_load -- (internal) loads and initializes all
 *	existing parts in the poolset directories
 */
static int
util_poolset_directories_load(struct pool_set *set)
{
	LOG(3, "set %p", set);

	if (!set->directory_based)
		return 0;

	unsigned next_part_id = 0;
	unsigned max_parts_rep = 0;
	for (unsigned r = 0; r < set->nreplicas; r++) {
		next_part_id = 0;

		struct pool_set_directory *d;
		int nparts = 0;
		int prev_nparts = 0;
		VEC_FOREACH_BY_PTR(d, &set->replica[r]->directory) {
			prev_nparts = nparts;
			nparts = util_poolset_directory_load(&set->replica[r],
				d->path);
			if (nparts < 0) {
				ERR("failed to load parts from directory %s",
					d->path);
				return -1;
			}

			next_part_id += (unsigned)nparts;

			/* always try to evenly spread files across dirs */
			if (r == 0 && prev_nparts > nparts)
				set->next_directory_id++;
		}

		if (next_part_id > set->replica[max_parts_rep]->nparts)
			max_parts_rep = r;

		if (r == 0)
			set->next_id = next_part_id;
	}

	/*
	 * In order to maintain the same semantics of poolset parsing for
	 * regular poolsets and directory poolsets, we need to speculatively
	 * recreate the information regarding any missing parts in replicas.
	 */
	struct pool_replica *rep;
	struct pool_replica *mrep = set->replica[max_parts_rep];

	for (unsigned r = 0; r < set->nreplicas; r++) {
		if (set->replica[r]->nparts == mrep->nparts)
			continue;

		if (VEC_SIZE(&set->replica[r]->directory) == 0) {
			errno = ENOENT;
			ERR("!no directories in replica");
			return -1;
		}

		if (util_replica_reserve(&set->replica[r], mrep->nparts) != 0)
			return -1;

		rep = set->replica[r];

		struct pool_set_directory *d = VEC_GET(&rep->directory, 0);

		for (unsigned pidx = 0; pidx < rep->nallocated; ++pidx) {
			struct pool_set_part *p = &rep->part[pidx];
			*p = mrep->part[pidx];

			size_t path_len = strlen(d->path) + PMEM_FILE_MAX_LEN;
			if ((p->path = Malloc(path_len)) == NULL) {
				ERR("!Malloc");
				return -1;
			}

			snprintf((char *)p->path, path_len,
					"%s" OS_DIR_SEP_STR "%0*u%s",
					d->path, PMEM_FILE_PADDING,
					pidx, PMEM_EXT);
		}
		rep->nparts = mrep->nparts;
	}

	return 0;
}

/*
 * util_poolset_parse -- parse pool set config file
 *
 * Returns 0 if the file is a valid poolset config file,
 * and -1 in case of any error.
 *
 * XXX: use memory mapped file
 */
int
util_poolset_parse(struct pool_set **setp, const char *path, int fd)
{
	LOG(3, "setp %p path %s fd %d", setp, path, fd);

	struct pool_set *set = NULL;
	enum parser_codes result;
	char *line;
	char *ppath;
	char *pool_desc;
	char *node_addr;
	char *cp;
	size_t psize;
	FILE *fs;
	int oerrno;

	if (os_lseek(fd, 0, SEEK_SET) != 0) {
		ERR("!lseek %d", fd);
		return -1;
	}

	fd = dup(fd);
	if (fd < 0) {
		ERR("!dup");
		return -1;
	}

	/* associate a stream with the file descriptor */
	if ((fs = os_fdopen(fd, "r")) == NULL) {
		ERR("!fdopen %d", fd);
		os_close(fd);
		return -1;
	}

	unsigned nlines = 0;
	unsigned nparts = 0; /* number of parts in current replica */

	/* read the first line */
	line = util_readline(fs);
	if (line == NULL) {
		ERR("!Reading poolset file");
		goto err;
	}
	nlines++;

	set = Zalloc(sizeof(struct pool_set));
	if (set == NULL) {
		ERR("!Malloc for pool set");
		goto err;
	}

	set->path = Strdup(path);
	if (set->path == NULL)  {
		ERR("!Strdup");
		goto err;
	}

	/* check also if the last character is '\n' */
	if (strncmp(line, POOLSET_HDR_SIG, POOLSET_HDR_SIG_LEN) == 0 &&
	    line[POOLSET_HDR_SIG_LEN] == '\n') {
		/* 'PMEMPOOLSET' signature detected */
		LOG(10, "PMEMPOOLSET");

		int ret = util_parse_add_replica(&set);
		if (ret != 0)
			goto err;

		nparts = 0;
		result = PARSER_CONTINUE;
	} else {
		result = PARSER_PMEMPOOLSET;
	}

	while (result == PARSER_CONTINUE) {
		Free(line);
		/* read next line */
		line = util_readline(fs);
		nlines++;

		if (line) {
			/* chop off newline and comments */
			if ((cp = strchr(line, '\n')) != NULL)
				*cp = '\0';
			if (cp != line && (cp = strchr(line, '#')) != NULL)
				*cp = '\0';

			/* skip comments and blank lines */
			if (cp == line)
				continue;
		}

		if (!line) {
			if (nparts >= 1) {
				result = PARSER_FORMAT_OK;
			} else {
				if (set->nreplicas == 1)
					result = PARSER_SET_NO_PARTS;
				else
					result = PARSER_REP_NO_PARTS;
			}
		} else if (strncmp(line, POOLSET_OPTION_SIG,
					POOLSET_OPTION_SIG_LEN) == 0) {
			result = parser_read_options(
					line + POOLSET_OPTION_SIG_LEN,
					&set->options);
			if (result == PARSER_CONTINUE) {
				LOG(10, "OPTIONS: %x", set->options);
			}
		} else if (strncmp(line, POOLSET_REPLICA_SIG,
					POOLSET_REPLICA_SIG_LEN) == 0) {
			if (line[POOLSET_REPLICA_SIG_LEN] != '\0') {
				/* something more than 'REPLICA' */
				char c = line[POOLSET_REPLICA_SIG_LEN];
				if (!isblank((unsigned char)c)) {
					result = PARSER_REPLICA;
					continue;
				}
				/* check if it is a remote replica */
				result = parser_read_replica(
						line + POOLSET_REPLICA_SIG_LEN,
						&node_addr, &pool_desc);
				if (result == PARSER_CONTINUE) {
					/* remote REPLICA */
					LOG(10, "REMOTE REPLICA "
						"node address '%s' "
						"pool set descriptor '%s'",
						node_addr, pool_desc);
					if (util_parse_add_remote_replica(&set,
							node_addr, pool_desc))
						goto err;
				}
			} else if (nparts >= 1) {
				/* 'REPLICA' signature detected */
				LOG(10, "REPLICA");

				int ret = util_parse_add_replica(&set);
				if (ret != 0)
					goto err;

				nparts = 0;
				result = PARSER_CONTINUE;
			} else {
				if (set->nreplicas == 1)
					result = PARSER_SET_NO_PARTS;
				else
					result = PARSER_REP_NO_PARTS;
			}
		} else {
			/* there could be no parts for remote replicas */
			if (set->replica[set->nreplicas - 1]->remote) {
				result = PARSER_REMOTE_REP_UNEXPECTED_PARTS;
				continue;
			}

			/* read size and path */
			result = parser_read_line(line, &psize, &ppath);
			if (result == PARSER_CONTINUE) {
				/* add a new pool's part to the list */
				int ret = util_parse_add_element(set,
					ppath, psize);
				if (ret != 0) {
					Free(ppath);
					goto err;
				}
				nparts++;
			}
		}
	}

	if (result != PARSER_FORMAT_OK) {
		ERR("%s [%s:%d]", path, parser_errstr[result], nlines);
		switch (result) {
		case PARSER_CANNOT_READ_SIZE:
		case PARSER_OUT_OF_MEMORY:
			/* do not overwrite errno */
			break;
		default:
			errno = EINVAL;
		}
		goto err;
	}

	if (util_poolset_check_devdax(set) != 0) {
		errno = EINVAL;
		goto err;
	}

	if (util_poolset_directories_load(set) != 0) {
		ERR("cannot load part files from directories");
		goto err;
	}

	LOG(4, "set file format correct (%s)", path);
	(void) os_fclose(fs);
	Free(line);
	util_poolset_check_options(set);
	util_poolset_set_size(set);
	*setp = set;
	return 0;

err:
	oerrno = errno;
	Free(line);
	(void) os_fclose(fs);
	if (set)
		util_poolset_free(set);
	errno = oerrno;
	return -1;
}

/*
 * util_poolset_single -- (internal) create a one-part pool set
 *
 * On success returns a pointer to a newly allocated and initialized
 * pool set structure.  Otherwise, NULL is returned.
 */
static struct pool_set *
util_poolset_single(const char *path, size_t filesize, int create,
	int ignore_sds)
{
	LOG(3, "path %s filesize %zu create %d",
			path, filesize, create);

	enum file_type type = util_file_get_type(path);
	if (type == OTHER_ERROR)
		return NULL;

	struct pool_set *set;
	set = Zalloc(sizeof(struct pool_set) +
			sizeof(struct pool_replica *));
	if (set == NULL) {
		ERR("!Malloc for pool set");
		return NULL;
	}

	set->path = Strdup(path);
	if (set->path == NULL)  {
		ERR("!Strdup");
		Free(set);
		return NULL;
	}

	struct pool_replica *rep;
	rep = Zalloc(sizeof(struct pool_replica) +
			sizeof(struct pool_set_part));
	if (rep == NULL) {
		ERR("!Malloc for pool set replica");
		Free(set->path);
		Free(set);
		return NULL;
	}

	VEC_INIT(&rep->directory);

	set->replica[0] = rep;

	rep->part[0].filesize = filesize;
	rep->part[0].path = Strdup(path);
	rep->part[0].fd = -1;	/* will be filled out by util_poolset_file() */
	rep->part[0].is_dev_dax = type == TYPE_DEVDAX;
	rep->part[0].created = create;
	rep->part[0].hdr = NULL;
	rep->part[0].addr = NULL;
	rep->part[0].has_bad_blocks = 0;

	if (rep->part[0].is_dev_dax)
		rep->part[0].alignment = util_file_device_dax_alignment(path);
	else
		rep->part[0].alignment = Mmap_align;

	ASSERTne(rep->part[0].alignment, 0);

	rep->nallocated = 1;
	rep->nparts = 1;
	rep->nhdrs = 1;

	/* it does not have a remote replica */
	rep->remote = NULL;
	set->remote = 0;

	/* round down to the nearest mapping alignment boundary */
	rep->repsize = rep->part[0].filesize & ~(rep->part[0].alignment - 1);
	rep->resvsize = rep->repsize;

	set->poolsize = rep->repsize;
	set->resvsize = rep->resvsize;

	set->nreplicas = 1;
	set->ignore_sds = ignore_sds || (set->options & OPTION_NOHDRS);

	return set;
}

/*
 * util_part_open -- open or create a single part file
 */
int
util_part_open(struct pool_set_part *part, size_t minsize, int create_part)
{
	LOG(3, "part %p minsize %zu create %d", part, minsize, create_part);

	int exists = util_file_exists(part->path);
	if (exists < 0)
		return -1;

	int create_file = create_part;

	if (exists)
		create_file = 0;

	part->created = 0;
	if (create_file) {
		part->fd = util_file_create(part->path, part->filesize,
				minsize);
		if (part->fd == -1) {
			LOG(2, "failed to create file: %s", part->path);
			return -1;
		}
		part->created = 1;
	} else {
		size_t size = 0;
		int flags = O_RDWR;
		part->fd = util_file_open(part->path, &size, minsize, flags);
		if (part->fd == -1) {
			LOG(2, "failed to open file: %s", part->path);
			return -1;
		}

		if (Fallocate_at_create && create_part && !part->is_dev_dax) {
			int ret = os_posix_fallocate(part->fd, 0,
					(os_off_t)size);
			if (ret != 0) {
				errno = ret;
				ERR("!posix_fallocate \"%s\", %zu", part->path,
					size);
				return -1;
			}
		}

		/* check if filesize matches */
		if (part->filesize != size) {
			ERR("file size does not match config: %s, %zu != %zu",
				part->path, size, part->filesize);
			errno = EINVAL;
			return -1;
		}
	}

	return 0;
}

/*
 * util_part_fdclose -- close part file
 */
void
util_part_fdclose(struct pool_set_part *part)
{
	LOG(3, "part %p", part);

	if (part->fd != -1) {
		(void) os_close(part->fd);
		part->fd = -1;
	}
}

/*
 * util_set_rpmem_attr -- (internal) overwrite existing pool attributes
 *
 * does not set uuid, next_part_uuid, prev_part_uuid
 */
static void
util_set_rpmem_attr(struct pool_hdr *hdrp, const struct rpmem_pool_attr *rattr)
{
	LOG(5, "hdrp %p rattr %p", hdrp, rattr);
	memcpy(hdrp->signature, rattr->signature, POOL_HDR_SIG_LEN);
	hdrp->major = rattr->major;
	hdrp->features.compat = rattr->compat_features;
	hdrp->features.incompat = rattr->incompat_features;
	hdrp->features.ro_compat = rattr->ro_compat_features;
	memcpy(hdrp->poolset_uuid, rattr->poolset_uuid, POOL_HDR_UUID_LEN);
	memcpy(hdrp->next_repl_uuid, rattr->next_uuid, POOL_HDR_UUID_LEN);
	memcpy(hdrp->prev_repl_uuid, rattr->prev_uuid, POOL_HDR_UUID_LEN);
	memcpy(&hdrp->arch_flags, rattr->user_flags, sizeof(struct arch_flags));
}

/*
 * util_get_rpmem_attr -- (internal) get attributes for remote replica header
 */
static void
util_get_rpmem_attr(struct rpmem_pool_attr *rattr, const struct pool_hdr *hdrp)
{
	LOG(5, "rpmem_attr %p hdrp %p", rattr, hdrp);
	ASSERTne(rattr, NULL);
	memcpy(rattr->signature, hdrp->signature, POOL_HDR_SIG_LEN);
	rattr->major = hdrp->major;
	rattr->compat_features = hdrp->features.compat;
	rattr->incompat_features = hdrp->features.incompat;
	rattr->ro_compat_features = hdrp->features.ro_compat;
	memcpy(rattr->poolset_uuid, hdrp->poolset_uuid, POOL_HDR_UUID_LEN);
	memcpy(rattr->uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
	memcpy(rattr->next_uuid, hdrp->next_repl_uuid, POOL_HDR_UUID_LEN);
	memcpy(rattr->prev_uuid, hdrp->prev_repl_uuid, POOL_HDR_UUID_LEN);
	memcpy(rattr->user_flags, &hdrp->arch_flags, sizeof(struct arch_flags));
}

/*
 * util_remote_store_attr -- (internal) store attributes read from remote
 *                           replica in the local volatile pool header
 */
static void
util_remote_store_attr(struct pool_hdr *hdrp,
		const struct rpmem_pool_attr *rattr)
{
	LOG(4, "hdrp %p rpmem_attr %p", hdrp, rattr);

	util_set_rpmem_attr(hdrp, rattr);
	memcpy(hdrp->uuid, rattr->uuid, POOL_HDR_UUID_LEN);
	memcpy(hdrp->next_part_uuid, rattr->uuid, POOL_HDR_UUID_LEN);
	memcpy(hdrp->prev_part_uuid, rattr->uuid, POOL_HDR_UUID_LEN);
}

/*
 * util_update_remote_header --  update attributes of a remote replica;
 *                               the remote replica must be open
 */
int
util_update_remote_header(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);

	ASSERTne(REP(set, repn)->remote, NULL);
	ASSERTne(REP(set, repn)->remote->rpp, NULL);

	struct pool_replica *rep = REP(set, repn);
	struct pool_hdr *hdr = HDR(rep, 0);

	/* get attributes from the local pool header */
	struct rpmem_pool_attr attributes;
	util_get_rpmem_attr(&attributes, hdr);

	/* push the attributes to the remote replica */
	RPMEMpool *rpp = rep->remote->rpp;
	int ret = Rpmem_set_attr(rpp, &attributes);
	if (ret) {
		ERR("!Rpmem_set_attr");
		return -1;
	}
	return 0;
}

/*
 * util_pool_close_remote -- close a remote replica
 */
int
util_pool_close_remote(RPMEMpool *rpp)
{
	LOG(3, "rpp %p", rpp);

	return Rpmem_close(rpp);
}

/*
 * util_poolset_remote_open -- open or create a remote replica
 */
int
util_poolset_remote_open(struct pool_replica *rep, unsigned repidx,
			size_t minsize, int create, void *pool_addr,
			size_t pool_size, unsigned *nlanes)
{
	LOG(3, "rep %p repidx %u minsize %zu create %d "
		"pool_addr %p pool_size %zu nlanes %p",
		rep, repidx, minsize, create,
		pool_addr, pool_size, nlanes);

	ASSERTne(nlanes, NULL);

	if (!Rpmem_handle_remote) {
		return -1;
	}

	unsigned remote_nlanes = *nlanes;

	if (create) {
		struct rpmem_pool_attr rpmem_attr_create;
		util_get_rpmem_attr(&rpmem_attr_create, rep->part[0].hdr);

		rep->remote->rpp = Rpmem_create(rep->remote->node_addr,
						rep->remote->pool_desc,
						pool_addr,
						pool_size,
						&remote_nlanes,
						&rpmem_attr_create);
		if (rep->remote->rpp == NULL) {
			ERR("creating remote replica #%u failed", repidx);
			return -1;
		}
		rep->part[0].created = 1;
	} else { /* open */
		struct rpmem_pool_attr rpmem_attr_open;

		rep->remote->rpp = Rpmem_open(rep->remote->node_addr,
						rep->remote->pool_desc,
						pool_addr,
						pool_size,
						&remote_nlanes,
						&rpmem_attr_open);
		if (rep->remote->rpp == NULL) {
			ERR("opening remote replica #%u failed", repidx);
			return -1;
		}

		util_remote_store_attr(rep->part[0].hdr, &rpmem_attr_open);
	}

	if (remote_nlanes < *nlanes)
		*nlanes = remote_nlanes;

	return 0;
}

/*
 * util_poolset_files_local -- (internal) open or create all the local
 *                              part files of a pool set and replica sets
 */
static int
util_poolset_files_local(struct pool_set *set, size_t minpartsize, int create)
{
	LOG(3, "set %p minpartsize %zu create %d", set, minpartsize, create);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		if (!rep->remote) {
			for (unsigned p = 0; p < rep->nparts; p++) {
				if (util_part_open(&rep->part[p], minpartsize,
						create))
					return -1;
			}
		}
	}

	return 0;
}

/*
 * util_poolset_remote_replica_open -- open remote replica
 */
int
util_poolset_remote_replica_open(struct pool_set *set, unsigned repidx,
	size_t minsize, int create, unsigned *nlanes)
{
#ifndef _WIN32
	/*
	 * This is a workaround for an issue with using device dax with
	 * libibverbs. To handle fork() function calls correctly libfabric use
	 * ibv_fork_init(3) which makes all registered memory being madvised
	 * with MADV_DONTFORK flag. In libpmemobj the remote replication is
	 * performed without pool header (first 4k). In such case the address
	 * passed to madvise(2) is aligned to 4k, but device dax can require
	 * different alignment (default is 2MB). This workaround madvises the
	 * entire memory region before registering it by fi_mr_reg(3).
	 *
	 * The librpmem client requires fork() support to work correctly.
	 */
	if (set->replica[0]->part[0].is_dev_dax) {
		int ret = os_madvise(set->replica[0]->part[0].addr,
				set->replica[0]->part[0].filesize,
				MADV_DONTFORK);
		if (ret) {
			ERR("!madvise");
			return ret;
		}
	}
#endif

	void *pool_addr = (void *)((uintptr_t)set->replica[0]->part[0].addr);

	return util_poolset_remote_open(set->replica[repidx], repidx, minsize,
			create, pool_addr, set->poolsize, nlanes);
}

/*
 * util_poolset_files_remote -- (internal) open or create all the remote
 *                              part files of a pool set and replica sets
 */
static int
util_poolset_files_remote(struct pool_set *set, size_t minsize,
				unsigned *nlanes, int create)
{
	LOG(3, "set %p minsize %zu nlanes %p create %d",
		set, minsize, nlanes, create);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote) {
			if (util_poolset_remote_replica_open(set, r,
				minsize, create, nlanes))
				return -1;
		}
	}

	return 0;
}

/*
 * util_poolset_read -- read memory pool set file
 *
 * On success returns 0 and a pointer to a newly allocated structure
 * containing the info of all the parts of the pool set and replicas.
 */
int
util_poolset_read(struct pool_set **setp, const char *path)
{
	LOG(3, "setp %p path %s", setp, path);

	int oerrno;
	int ret = 0;
	int fd;

	if ((fd = os_open(path, O_RDONLY)) < 0) {
		ERR("!open: path \"%s\"", path);
		return -1;
	}

	ret = util_poolset_parse(setp, path, fd);

	oerrno = errno;
	(void) os_close(fd);
	errno = oerrno;
	return ret;
}

/*
 * util_poolset_create_set -- create a new pool set structure
 *
 * On success returns 0 and a pointer to a newly allocated structure
 * containing the info of all the parts of the pool set and replicas.
 */
int
util_poolset_create_set(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize, int ignore_sds)
{
	LOG(3, "setp %p path %s poolsize %zu minsize %zu",
		setp, path, poolsize, minsize);

	int oerrno;
	int ret = 0;
	int fd;
	size_t size = 0;

	enum file_type type = util_file_get_type(path);
	if (type == OTHER_ERROR)
		return -1;

	if (poolsize != 0) {
		if (type == TYPE_DEVDAX) {
			ERR("size must be zero for device dax");
			return -1;
		}
		*setp = util_poolset_single(path, poolsize, 1, ignore_sds);
		if (*setp == NULL)
			return -1;

		return 0;
	}

	/* do not check minsize */
	if ((fd = util_file_open(path, &size, 0, O_RDONLY)) == -1)
		return -1;

	char signature[POOLSET_HDR_SIG_LEN];
	if (type == TYPE_NORMAL) {
		/*
		 * read returns ssize_t, but we know it will return value
		 * between -1 and POOLSET_HDR_SIG_LEN (11), so we can safely
		 * cast it to int
		 */
		ret = (int)read(fd, signature, POOLSET_HDR_SIG_LEN);
		if (ret < 0) {
			ERR("!read %d", fd);
			goto err;
		}
	}

	if (type == TYPE_DEVDAX || ret < POOLSET_HDR_SIG_LEN ||
	    strncmp(signature, POOLSET_HDR_SIG, POOLSET_HDR_SIG_LEN)) {
		LOG(4, "not a pool set header");
		(void) os_close(fd);

		if (size < minsize) {
			ERR("file is not a poolset file and its size (%zu)"
				" is smaller than %zu", size, minsize);
			errno = EINVAL;
			return -1;
		}
		*setp = util_poolset_single(path, size, 0, ignore_sds);
		if (*setp == NULL)
			return -1;

		return 0;
	}

	ret = util_poolset_parse(setp, path, fd);
	if (ret)
		goto err;

	(*setp)->ignore_sds = ignore_sds || ((*setp)->options & OPTION_NOHDRS);
#ifdef _WIN32
	/* remote replication is not supported on Windows */
	if ((*setp)->remote) {
		util_poolset_free(*setp);
		ERR("remote replication is not supported on Windows");
		errno = ENOTSUP;
		ret = -1;
		goto err;
	}
#endif /* _WIN32 */

err:
	oerrno = errno;
	(void) os_close(fd);
	errno = oerrno;
	return ret;
}

/*
 * util_poolset_check_header_options -- (internal) check if poolset options
 *                                      match given flags
 */
static int
util_poolset_check_header_options(struct pool_set *set, uint32_t incompat)
{
	LOG(3, "set %p, incompat %#x", set, incompat);

	if (((set->options & OPTION_SINGLEHDR) == 0) !=
			((incompat & POOL_FEAT_SINGLEHDR) == 0)) {
		ERR(
			"poolset file options (%u) do not match incompat feature flags (%#x)",
			set->options, incompat);
		errno = EINVAL;
		return -1;
	}
	return 0;
}

/*
 * util_header_create -- create header of a single pool set file
 */
int
util_header_create(struct pool_set *set, unsigned repidx, unsigned partidx,
	const struct pool_attr *attr, int overwrite)
{
	LOG(3, "set %p repidx %u partidx %u attr %p overwrite %d", set, repidx,
		partidx, attr, overwrite);

	ASSERTne(attr, NULL);

	struct pool_replica *rep = set->replica[repidx];

	/* opaque info lives at the beginning of mapped memory pool */
	struct pool_hdr *hdrp = rep->part[partidx].hdr;

	/* check if the pool header is all zeros */
	if (!util_is_zeroed(hdrp, sizeof(*hdrp)) && !overwrite) {
		ERR("Non-empty file detected");
		errno = EEXIST;
		return -1;
	}

	/* create pool's header */
	util_pool_attr2hdr(hdrp, attr);

	if (set->options & OPTION_SINGLEHDR)
		hdrp->features.incompat |= POOL_FEAT_SINGLEHDR;

	memcpy(hdrp->poolset_uuid, set->uuid, POOL_HDR_UUID_LEN);
	memcpy(hdrp->uuid, PART(rep, partidx)->uuid, POOL_HDR_UUID_LEN);

	/* link parts */
	if (set->options & OPTION_SINGLEHDR) {
		/* next/prev part point to part #0 */
		ASSERTeq(partidx, 0);
		memcpy(hdrp->prev_part_uuid, PART(rep, 0)->uuid,
							POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_part_uuid, PART(rep, 0)->uuid,
							POOL_HDR_UUID_LEN);
	} else {
		memcpy(hdrp->prev_part_uuid, PARTP(rep, partidx)->uuid,
							POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_part_uuid, PARTN(rep, partidx)->uuid,
							POOL_HDR_UUID_LEN);
	}

	/* link replicas */
	if (!util_is_zeroed(attr->prev_repl_uuid, POOL_HDR_UUID_LEN)) {
		memcpy(hdrp->prev_repl_uuid, attr->prev_repl_uuid,
				POOL_HDR_UUID_LEN);
	} else {
		memcpy(hdrp->prev_repl_uuid, PART(REPP(set, repidx), 0)->uuid,
			POOL_HDR_UUID_LEN);
	}
	if (!util_is_zeroed(attr->next_repl_uuid, POOL_HDR_UUID_LEN)) {
		memcpy(hdrp->next_repl_uuid, attr->next_repl_uuid,
				POOL_HDR_UUID_LEN);
	} else {
		memcpy(hdrp->next_repl_uuid, PART(REPN(set, repidx), 0)->uuid,
			POOL_HDR_UUID_LEN);
	}

	if (!rep->remote) {
		os_stat_t stbuf;

		if (os_fstat(rep->part[partidx].fd, &stbuf) != 0) {
			ERR("!fstat");
			return -1;
		}
		ASSERT(stbuf.st_ctime);
		hdrp->crtime = (uint64_t)stbuf.st_ctime;
	}

	int arch_is_zeroed = util_is_zeroed(attr->arch_flags,
			POOL_HDR_ARCH_LEN);
	if (arch_is_zeroed)
		util_get_arch_flags(&hdrp->arch_flags);

	util_convert2le_hdr(hdrp);

	if (!arch_is_zeroed) {
		memcpy(&hdrp->arch_flags, attr->arch_flags, POOL_HDR_ARCH_LEN);
	}

	if (!set->ignore_sds && partidx == 0 && !rep->remote) {
		shutdown_state_init(&hdrp->sds, rep);
		for (unsigned p = 0; p < rep->nparts; p++) {
			if (shutdown_state_add_part(&hdrp->sds,
					PART(rep, p)->path, rep))
				return -1;
		}
		shutdown_state_set_dirty(&hdrp->sds, rep);
	}

	util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
		1, POOL_HDR_CSUM_END_OFF(hdrp));

	/* store pool's header */
	util_persist_auto(rep->is_pmem, hdrp, sizeof(*hdrp));

	return 0;
}

/*
 * util_header_check -- (internal) validate header of a single pool set file
 */
static int
util_header_check(struct pool_set *set, unsigned repidx, unsigned partidx,
	const struct pool_attr *attr)
{
	LOG(3, "set %p repidx %u partidx %u attr %p", set, repidx, partidx,
			attr);

	ASSERTne(attr, NULL);

	struct pool_replica *rep = set->replica[repidx];

	/* opaque info lives at the beginning of mapped memory pool */
	struct pool_hdr *hdrp = rep->part[partidx].hdr;
	struct pool_hdr hdr;

	memcpy(&hdr, hdrp, sizeof(hdr));

	/* local copy of a remote header does not need to be converted */
	if (rep->remote == NULL)
		util_convert2h_hdr_nocheck(&hdr);

	/* to be valid, a header must have a major version of at least 1 */
	if (hdr.major == 0) {
		ERR("invalid major version (0)");
		errno = EINVAL;
		return -1;
	}

	/* check signature */
	if (memcmp(hdr.signature, attr->signature, POOL_HDR_SIG_LEN)) {
		ERR("wrong pool type: \"%.8s\"", hdr.signature);
		errno = EINVAL;
		return -1;
	}

	/* check format version number */
	if (hdr.major != attr->major) {
		ERR("pool version %d (library expects %d)", hdr.major,
				attr->major);
		if (hdr.major < attr->major)
			ERR(
				"Please run the pmdk-convert utility to upgrade the pool.");
		errno = EINVAL;
		return -1;
	}

	rep->part[partidx].rdonly = 0;

	int retval = util_feature_check(&hdr, attr->features);
	if (retval < 0)
		return -1;

	if (retval == 0)
		rep->part[partidx].rdonly = 1;

	if (rep->remote == NULL) {
		/*
		 * and to be valid, the fields must checksum correctly
		 *
		 * NOTE: checksum validation is performed after format version
		 * and feature check, because if POOL_FEAT_CKSUM_2K flag is set,
		 * we want to report it as incompatible feature, rather than
		 * invalid checksum.
		 */
		if (!util_checksum(&hdr, sizeof(hdr), &hdr.checksum,
				0, POOL_HDR_CSUM_END_OFF(&hdr))) {
			ERR("invalid checksum of pool header");
			errno = EINVAL;
			return -1;
		}

		LOG(3, "valid header, signature \"%.8s\"", hdr.signature);
	}

	if (util_check_arch_flags(&hdr.arch_flags)) {
		ERR("wrong architecture flags");
		errno = EINVAL;
		return -1;
	}

	/* check pool set UUID */
	if (memcmp(HDR(REP(set, 0), 0)->poolset_uuid, hdr.poolset_uuid,
						POOL_HDR_UUID_LEN)) {
		ERR("wrong pool set UUID");
		errno = EINVAL;
		return -1;
	}

	/* check pool set linkage */
	if (memcmp(HDRP(rep, partidx)->uuid, hdr.prev_part_uuid,
						POOL_HDR_UUID_LEN) ||
	    memcmp(HDRN(rep, partidx)->uuid, hdr.next_part_uuid,
						POOL_HDR_UUID_LEN)) {
		ERR("wrong part UUID");
		errno = EINVAL;
		return -1;
	}

	/* check format version */
	if (HDR(rep, 0)->major != hdrp->major) {
		ERR("incompatible pool format");
		errno = EINVAL;
		return -1;
	}

	/* check compatibility features */
	if (HDR(rep, 0)->features.compat != hdrp->features.compat ||
	    HDR(rep, 0)->features.incompat != hdrp->features.incompat ||
	    HDR(rep, 0)->features.ro_compat != hdrp->features.ro_compat) {
		ERR("incompatible feature flags");
		errno = EINVAL;
		return -1;
	}

	/* check poolset options */
	if (util_poolset_check_header_options(set,
			HDR(rep, 0)->features.incompat))
		return -1;

	return 0;
}

/*
 * util_header_check_remote -- (internal) validate header of a remote
 *                             pool set file
 */
static int
util_header_check_remote(struct pool_set *set, unsigned partidx)
{
	LOG(3, "set %p partidx %u ", set, partidx);

	/* there is only one replica in remote poolset */
	struct pool_replica *rep = set->replica[0];
	/* opaque info lives at the beginning of mapped memory pool */
	struct pool_hdr *hdrp = rep->part[partidx].hdr;
	struct pool_hdr hdr;

	if (util_is_zeroed(hdrp, sizeof(*hdrp))) {
		ERR("pool header zeroed");
		errno = EINVAL;
		return -1;
	}

	memcpy(&hdr, hdrp, sizeof(hdr));

	util_convert2h_hdr_nocheck(&hdr);

	/* valid header found */
	if (memcmp(HDR(rep, 0)->signature, hdrp->signature, POOL_HDR_SIG_LEN)) {
		ERR("pool signature mismatch in part %d", partidx);
		errno = EINVAL;
		return -1;
	}

	/* check format version */
	if (HDR(rep, 0)->major != hdrp->major) {
		ERR("pool version mismatch in part %d", partidx);
		errno = EINVAL;
		return -1;
	}

	/* check compatibility features */
	if (HDR(rep, 0)->features.compat != hdrp->features.compat) {
		ERR("'may have' compatibility flags mismatch in part %d",
								partidx);
		errno = EINVAL;
		return -1;
	}
	if (HDR(rep, 0)->features.incompat != hdrp->features.incompat) {
		ERR("'must support' compatibility flags mismatch in part %d",
								partidx);
		errno = EINVAL;
		return -1;
	}
	if (HDR(rep, 0)->features.ro_compat != hdrp->features.ro_compat) {
		ERR("'force read-only' compatibility flags mismatch in part %d",
								partidx);
		errno = EINVAL;
		return -1;
	}

	/*
	 * and to be valid, the fields must checksum correctly
	 *
	 * NOTE: checksum validation is performed after format version and
	 * feature check, because if POOL_FEAT_CKSUM_2K flag is set,
	 * we want to report it as incompatible feature, rather than invalid
	 * checksum.
	 */
	if (!util_checksum(&hdr, sizeof(hdr), &hdr.checksum,
			0, POOL_HDR_CSUM_END_OFF(&hdr))) {
		ERR("invalid checksum of pool header");
		return -1;
	}

	LOG(3, "valid header, signature \"%.8s\"", hdr.signature);

	/* check pool set UUID */
	if (memcmp(HDR(rep, 0)->poolset_uuid, hdrp->poolset_uuid,
							POOL_HDR_UUID_LEN)) {
		ERR("wrong pool set UUID in part %d", partidx);
		errno = EINVAL;
		return -1;
	}

	/* check previous replica UUID */
	if (memcmp(HDR(rep, 0)->prev_repl_uuid, hdrp->prev_repl_uuid,
							POOL_HDR_UUID_LEN)) {
		ERR("wrong previous replica UUID in part %d", partidx);
		errno = EINVAL;
		return -1;
	}

	/* check next replica UUID */
	if (memcmp(HDR(rep, 0)->next_repl_uuid, hdrp->next_repl_uuid,
						POOL_HDR_UUID_LEN)) {
		ERR("wrong next replica UUID in part %d", partidx);
		errno = EINVAL;
		return -1;
	}

	if (memcmp(&HDR(rep, 0)->arch_flags, &hdrp->arch_flags,
						sizeof(hdrp->arch_flags))) {
		ERR("wrong architecture flags");
		errno = EINVAL;
		return -1;
	}

	/* check pool set linkage */
	if (memcmp(HDRP(rep, partidx)->uuid, hdrp->prev_part_uuid,
							POOL_HDR_UUID_LEN) ||
	    memcmp(HDRN(rep, partidx)->uuid, hdrp->next_part_uuid,
							POOL_HDR_UUID_LEN)) {
		ERR("wrong part UUID in part %d", partidx);
		errno = EINVAL;
		return -1;
	}

	/* read shutdown state toggle from header */
	set->ignore_sds |= IGNORE_SDS(HDR(rep, 0));

	if (!set->ignore_sds && partidx == 0) {
		struct shutdown_state sds;
		shutdown_state_init(&sds, NULL);
		for (unsigned p = 0; p < rep->nparts; p++) {
			if (shutdown_state_add_part(&sds,
					PART(rep, p)->path, NULL))
				return -1;
		}

		if (shutdown_state_check(&sds, &hdrp->sds, rep)) {
			errno = EINVAL;
			return -1;
		}

		shutdown_state_set_dirty(&hdrp->sds, rep);
	}

	rep->part[partidx].rdonly = 0;

	return 0;
}

/*
 * util_replica_set_is_pmem -- sets per-replica is_pmem flag
 *
 * The replica is PMEM if:
 * - all parts are on device dax, or
 * - all parts are mapped with MAP_SYNC.
 *
 * It's enough to check only first part because it's already verified
 * that either all or none parts are device dax or mapped with MAP_SYNC.
 */
static inline void
util_replica_set_is_pmem(struct pool_replica *rep)
{
	rep->is_pmem = rep->part[0].is_dev_dax || rep->part[0].map_sync ||
		pmem_is_pmem(rep->part[0].addr, rep->resvsize);
}

/*
 * util_replica_map_local -- (internal) map memory pool for local replica
 */
static int
util_replica_map_local(struct pool_set *set, unsigned repidx, int flags)
{
	LOG(3, "set %p repidx %u flags %d", set, repidx, flags);

	/*
	 * XXX: Like we reserve space for all parts in this replica when we map
	 * the first part, we need to reserve the space for all replicas
	 * upfront.  It is not necessary that the replicas are contiguous but
	 * that way we would not fragment the memory much.  I think we should
	 * leave this to MM, but let's have a note as per our collective minds.
	 */

#ifndef _WIN32
	int remaining_retries = 0;
#else
	int remaining_retries = 10;
#endif
	int retry_for_contiguous_addr;
	size_t mapsize;
	/* header size for all headers but the first one */
	size_t hdrsize = (set->options & (OPTION_SINGLEHDR | OPTION_NOHDRS)) ?
			0 : Mmap_align;
	void *addr;
	struct pool_replica *rep = set->replica[repidx];

	ASSERTeq(rep->remote, NULL);
	ASSERTne(rep->part, NULL);

	do {
		retry_for_contiguous_addr = 0;
		mapsize = rep->part[0].filesize & ~(Mmap_align - 1);

		/* determine a hint address for mmap() */
		addr = util_map_hint(rep->resvsize, 0);
		if (addr == MAP_FAILED) {
			LOG(1, "cannot find a contiguous region of given size");
			return -1;
		}

		/* map the first part and reserve space for remaining parts */
		if (util_map_part(&rep->part[0], addr, rep->resvsize, 0,
				flags, 0) != 0) {
			LOG(2, "pool mapping failed - replica #%u part #0",
				repidx);
			return -1;
		}

		VALGRIND_REGISTER_PMEM_MAPPING(rep->part[0].addr,
				rep->part[0].size);
		VALGRIND_REGISTER_PMEM_FILE(rep->part[0].fd,
				rep->part[0].addr, rep->part[0].size, 0);

		set->zeroed &= rep->part[0].created;

		addr = (char *)rep->part[0].addr + mapsize;

		/*
		 * map the remaining parts of the usable pool space
		 * (aligned to memory mapping granularity)
		 */
		for (unsigned p = 1; p < rep->nparts; p++) {
			/* map data part */
			if (util_map_part(&rep->part[p], addr, 0, hdrsize,
					flags | MAP_FIXED, 0) != 0) {
				/*
				 * if we can't map the part at the address we
				 * asked for, unmap all the parts that are
				 * mapped and remap at a different address.
				 */
				if ((errno == EINVAL) &&
				    (remaining_retries > 0)) {
					LOG(2, "usable space mapping failed - "
						"part #%d - retrying", p);
					retry_for_contiguous_addr = 1;
					remaining_retries--;

					util_unmap_parts(rep, 0, p - 1);

					/* release rest of the VA reserved */
					ASSERTne(addr, NULL);
					ASSERTne(addr, MAP_FAILED);
					munmap(addr, rep->resvsize - mapsize);
					break;
				}
				LOG(2, "usable space mapping failed - part #%d",
					p);
				goto err;
			}

			VALGRIND_REGISTER_PMEM_FILE(rep->part[p].fd,
				rep->part[p].addr, rep->part[p].size,
				hdrsize);

			mapsize += rep->part[p].size;
			set->zeroed &= rep->part[p].created;
			addr = (char *)addr + rep->part[p].size;
		}
	} while (retry_for_contiguous_addr);

	/*
	 * Initially part[0].size is the size of address space
	 * reservation for all parts from given replica. After
	 * mapping that space we need to overwrite part[0].size
	 * with its actual size to be consistent - size for each
	 * part should be the actual mapping size of this part
	 * only - it simplifies future calculations.
	 */
	rep->part[0].size = rep->part[0].filesize & ~(Mmap_align - 1);

	if (util_replica_check_map_sync(set, repidx, 0))
		goto err;

	util_replica_set_is_pmem(rep);

	if (Prefault_at_create)
		util_replica_force_page_allocation(rep);

	ASSERTeq(mapsize, rep->repsize);

	LOG(3, "replica #%u addr %p", repidx, rep->part[0].addr);

	return 0;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (mapsize < rep->repsize) {
		ASSERTne(rep->part[0].addr, NULL);
		ASSERTne(rep->part[0].addr, MAP_FAILED);
		munmap(rep->part[0].addr, rep->resvsize - mapsize);
	}
	for (unsigned p = 0; p < rep->nparts; p++) {
		util_unmap_part(&rep->part[p]);
	}
	errno = oerrno;
	return -1;
}

/*
 * util_replica_init_headers_local -- (internal) initialize pool headers
 */
static int
util_replica_init_headers_local(struct pool_set *set, unsigned repidx,
	int flags, const struct pool_attr *attr)
{
	LOG(3, "set %p repidx %u flags %d attr %p", set, repidx, flags, attr);

	struct pool_replica *rep = set->replica[repidx];

	/* map all headers - don't care about the address */
	for (unsigned p = 0; p < rep->nhdrs; p++) {
		if (util_map_hdr(&rep->part[p], flags, 0) != 0) {
			LOG(2, "header mapping failed - part #%d", p);
			goto err;
		}
	}

	/* create headers, set UUID's */
	for (unsigned p = 0; p < rep->nhdrs; p++) {
		if (util_header_create(set, repidx, p, attr, 0) != 0) {
			LOG(2, "header creation failed - part #%d", p);
			goto err;
		}
	}

	/* unmap all headers */
	for (unsigned p = 0; p < rep->nhdrs; p++)
		util_unmap_hdr(&rep->part[p]);

	return 0;

err:
	LOG(4, "error clean up");
	int oerrno = errno;
	for (unsigned p = 0; p < rep->nhdrs; p++) {
		util_unmap_hdr(&rep->part[p]);
	}
	errno = oerrno;
	return -1;
}

/*
 * util_replica_create_local -- (internal) create a new memory pool for local
 * replica
 */
static int
util_replica_create_local(struct pool_set *set, unsigned repidx, int flags,
	const struct pool_attr *attr)
{
	LOG(3, "set %p repidx %u flags %d attr %p", set, repidx, flags, attr);

	/*
	 * the first replica has to be mapped prior to remote ones so if
	 * a replica is already mapped skip mapping creation
	 */
	if (PART(REP(set, repidx), 0)->addr == NULL) {
		if (util_replica_map_local(set, repidx, flags) != 0) {
			LOG(2, "replica #%u map failed", repidx);
			return -1;
		}
	}

	if (attr == NULL)
		return 0;

	if (util_replica_init_headers_local(set, repidx, flags, attr) != 0) {
		LOG(2, "replica #%u headers initialization failed", repidx);
		return -1;
	}
	return 0;
}

/*
 * util_replica_create_remote -- (internal) create a new memory pool
 *                               for remote replica
 */
static int
util_replica_create_remote(struct pool_set *set, unsigned repidx, int flags,
	const struct pool_attr *attr)
{
	LOG(3, "set %p repidx %u flags %d attr %p", set, repidx, flags, attr);

	struct pool_replica *rep = set->replica[repidx];

	ASSERTne(rep->remote, NULL);
	ASSERTne(rep->part, NULL);
	ASSERTeq(rep->nparts, 1);
	ASSERTeq(rep->nhdrs, 1);
	ASSERTne(attr, NULL);

	struct pool_set_part *part = rep->part;

	/*
	 * A remote replica has one fake part of size equal twice pool header
	 * size for storing pool header and pool descriptor.
	 */
	part->size = rep->repsize;
	ASSERT(IS_PAGE_ALIGNED(part->size));
	part->remote_hdr = Zalloc(part->size + Pagesize);
	if (!part->remote_hdr) {
		ERR("!Zalloc");
		return -1;
	}

	part->hdr = PAGE_ALIGN_UP(part->remote_hdr);
	part->addr = PAGE_ALIGN_UP(part->remote_hdr);
	part->hdrsize = POOL_HDR_SIZE;

	/* create header, set UUID's */
	if (util_header_create(set, repidx, 0, attr, 0) != 0) {
		LOG(2, "header creation failed - part #0");
		Free(part->remote_hdr);
		return -1;
	}

	LOG(3, "replica #%u addr %p", repidx, rep->part[0].addr);

	return 0;
}

/*
 * util_replica_close -- close a memory pool replica
 *
 * This function unmaps all mapped memory regions.
 */
int
util_replica_close(struct pool_set *set, unsigned repidx)
{
	LOG(3, "set %p repidx %u", set, repidx);
	struct pool_replica *rep = set->replica[repidx];

	if (rep->remote == NULL) {
		struct pool_set_part *part = PART(rep, 0);
		if (!set->ignore_sds && part->addr != NULL &&
				part->size != 0) {
			struct pool_hdr *hdr = part->addr;
			RANGE_RW(hdr, sizeof(*hdr), part->is_dev_dax);
			/*
			 * deep drain will call msync on one page in each
			 * part in replica to trigger WPQ flush.
			 * This pages may have been marked as
			 * undefined/inaccessible, but msyncing such memory
			 * is not a bug, so as a workaround temporarily
			 * disable error reporting.
			 */
			VALGRIND_DO_DISABLE_ERROR_REPORTING;
			util_replica_deep_drain(part->addr, rep->repsize,
				set, repidx);
			VALGRIND_DO_ENABLE_ERROR_REPORTING;
			shutdown_state_clear_dirty(&hdr->sds, rep);
		}
		for (unsigned p = 0; p < rep->nhdrs; p++)
			util_unmap_hdr(&rep->part[p]);

		rep->part[0].size = rep->resvsize;
		util_unmap_part(&rep->part[0]);
	} else {
		LOG(4, "freeing volatile header of remote replica #%u", repidx);
		Free(rep->part[0].remote_hdr);
		rep->part[0].remote_hdr = NULL;
		rep->part[0].hdr = NULL;
		rep->part[0].hdrsize = 0;
		rep->part[0].addr = NULL;
		rep->part[0].size = 0;
	}

	return 0;
}

/*
 * util_poolset_append_new_part -- (internal) creates a new part in each replica
 *	of the poolset
 */
static int
util_poolset_append_new_part(struct pool_set *set, size_t size)
{
	LOG(3, "set %p size %zu", set, size);

	if (!set->directory_based)
		return -1;

	struct pool_set_directory *d;
	size_t directory_id;
	char *path;
	size_t path_len;

	unsigned r;
	for (r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];

		directory_id = set->next_directory_id %
			VEC_SIZE(&rep->directory);
		d = VEC_GET(&rep->directory, directory_id);

		path_len = strlen(d->path) + PMEM_FILE_MAX_LEN;
		if ((path = Malloc(path_len)) == NULL) {
			ERR("!Malloc");
			goto err_part_init;
		}

		snprintf(path, path_len, "%s" OS_DIR_SEP_STR "%0*u%s",
			d->path, PMEM_FILE_PADDING, set->next_id, PMEM_EXT);

		if (util_replica_add_part(&set->replica[r], path, size) != 0)
			FATAL("cannot add a new part to the replica info");
	}

	set->next_directory_id += 1;
	set->next_id += 1;

	util_poolset_set_size(set);

	return 0;

err_part_init:
	/* for each replica 0..r-1 remove the last part */
	for (unsigned rn = 0; rn < r; ++rn) {
		struct pool_replica *rep = set->replica[rn];
		unsigned pidx = rep->nparts - 1;
		Free((void *)(rep->part[pidx].path));
		rep->part[pidx].path = NULL;
		rep->nparts--;
	}

	return -1;
}

/*
 * util_pool_extend -- extends the poolset by the provided size
 */
void *
util_pool_extend(struct pool_set *set, size_t *size, size_t minpartsize)
{
	LOG(3, "set %p size %zu minpartsize %zu", set, *size, minpartsize);

	if (*size == 0) {
		ERR("cannot extend pool by 0 bytes");
		return NULL;
	}

	if ((set->options & OPTION_SINGLEHDR) == 0) {
		ERR(
		"extending the pool by appending parts with headers is not supported!");
		return NULL;
	}

	if (set->poolsize + *size > set->resvsize) {
		*size = set->resvsize - set->poolsize;
		if (*size < minpartsize) {
			ERR("exceeded reservation size");
			return NULL;
		}
		LOG(4, "extend size adjusted to not exceed reservation size");
	}

	size_t old_poolsize = set->poolsize;

	if (util_poolset_append_new_part(set, *size) != 0) {
		ERR("unable to append a new part to the pool");
		return NULL;
	}

	size_t hdrsize = (set->options & OPTION_SINGLEHDR) ? 0 : Mmap_align;
	void *addr = NULL;
	void *addr_base = NULL;

	unsigned r;
	for (r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		unsigned pidx = rep->nparts - 1;
		struct pool_set_part *p = &rep->part[pidx];

		if (util_part_open(p, 0, 1 /* create */) != 0) {
			ERR("cannot open the new part");
			goto err;
		}

		addr = (char *)rep->part[0].addr + old_poolsize;
		if (addr_base == NULL)
			addr_base = addr;

		if (util_map_part(p, addr, 0, hdrsize,
				MAP_SHARED | MAP_FIXED, 0) != 0) {
			ERR("cannot map the new part");
			goto err;
		}

		/*
		 * new part must be mapped the same way as all the rest
		 * within a replica
		 */
		if (p->map_sync != rep->part[0].map_sync) {
			if (p->map_sync)
				ERR("new part cannot be mapped with MAP_SYNC");
			else
				ERR("new part mapped with MAP_SYNC");
			goto err;
		}
	}

	/* XXX: mode should be the same as for pmemxxx_create() */
	if (util_poolset_chmod(set, S_IWUSR | S_IRUSR))
		goto err;

	util_poolset_fdclose(set);

	return addr_base;

err:
	for (unsigned rn = 0; rn <= r; ++rn) {
		struct pool_replica *rep = set->replica[r];
		unsigned pidx = rep->nparts - 1;
		struct pool_set_part *p = &rep->part[pidx];
		rep->nparts--;

		if (p->fd != 0)
			(void) os_close(p->fd);
		if (p->created)
			os_unlink(p->path);
		Free((void *)p->path);
		p->path = NULL;
	}
	util_poolset_set_size(set);

	return NULL;
}

/*
 * util_print_bad_files_cb -- (internal) callback printing names of pool files
 *                            containing bad blocks
 */
static int
util_print_bad_files_cb(struct part_file *pf, void *arg)
{
	if (!pf->is_remote && pf->part && pf->part->has_bad_blocks)
		ERR("file contains bad blocks -- '%s'", pf->part->path);

	return 0;
}

/*
 * util_pool_create_uuids -- create a new memory pool (set or a single file)
 *                           with given uuids
 *
 * On success returns 0 and a pointer to a newly allocated structure
 * containing the info of all the parts of the pool set and replicas.
 */
int
util_pool_create_uuids(struct pool_set **setp, const char *path,
	size_t poolsize, size_t minsize, size_t minpartsize,
	const struct pool_attr *attr, unsigned *nlanes, int can_have_rep,
	int remote)
{
	LOG(3, "setp %p path %s poolsize %zu minsize %zu minpartsize %zu "
		"pattr %p nlanes %p can_have_rep %i remote %i", setp, path,
		poolsize, minsize, minpartsize, attr, nlanes, can_have_rep,
		remote);

	/* attributes cannot be NULL for local replicas */
	ASSERT(remote || attr != NULL);

	int flags = MAP_SHARED;
	int oerrno;

	int exists = util_file_exists(path);
	if (exists < 0)
		return -1;

	/* check if file exists */
	if (poolsize > 0 && exists) {
		ERR("file %s already exists", path);
		errno = EEXIST;
		return -1;
	}

	int ret = util_poolset_create_set(setp, path, poolsize, minsize,
			IGNORE_SDS(attr));
	if (ret < 0) {
		LOG(2, "cannot create pool set -- '%s'", path);
		return -1;
	}

	struct pool_set *set = *setp;

	ASSERT(set->nreplicas > 0);

	if (!remote && (set->options & OPTION_NOHDRS)) {
		ERR(
			"the NOHDRS poolset option is not supported for local poolsets");
		errno = EINVAL;
		goto err_poolset_free;
	}

	if ((attr == NULL) != ((set->options & OPTION_NOHDRS) != 0)) {
		ERR(
			"pool attributes are not supported for poolsets without headers (with the NOHDRS option)");
		errno = EINVAL;
		goto err_poolset_free;
	}

	if (set->directory_based && ((set->options & OPTION_SINGLEHDR) == 0)) {
		ERR(
			"directory based pools are not supported for poolsets with headers (without SINGLEHDR option)");
		errno = EINVAL;
		goto err_poolset_free;
	}

	if (set->resvsize < minsize) {
		ERR("reservation pool size %zu smaller than %zu",
			set->resvsize, minsize);
		errno = EINVAL;
		goto err_poolset_free;
	}

	if (set->directory_based && set->poolsize == 0 &&
			util_poolset_append_new_part(set, minsize) != 0) {
		ERR("cannot create a new part in provided directories");
		goto err_poolset_free;
	}

	if (attr != NULL &&
	    (attr->features.compat & POOL_FEAT_CHECK_BAD_BLOCKS)) {
		int bbs = badblocks_check_poolset(set, 1 /* create */);
		if (bbs < 0) {
			LOG(1,
				"failed to check pool set for bad blocks -- '%s'",
				path);
			goto err_poolset_free;
		}

		if (bbs > 0) {
			util_poolset_foreach_part_struct(set,
							util_print_bad_files_cb,
							NULL);
			ERR(
				"pool set contains bad blocks and cannot be created, run 'pmempool create --clear-bad-blocks' utility to clear bad blocks and create a pool");
			errno = EIO;
			goto err_poolset_free;
		}
	}

	if (set->poolsize < minsize) {
		ERR("net pool size %zu smaller than %zu",
			set->poolsize, minsize);
		errno = EINVAL;
		goto err_poolset_free;
	}

	if (remote) {
		/* it is a remote replica - it cannot have replicas */
		if (set->nreplicas > 1) {
			LOG(2, "remote pool set cannot have replicas");
			errno = EINVAL;
			goto err_poolset_free;
		}

		/* check if poolset options match remote pool attributes */
		if (attr != NULL &&
				((set->options & OPTION_SINGLEHDR) == 0) !=
				((attr->features.incompat &
						POOL_FEAT_SINGLEHDR) == 0)) {
			ERR(
				"pool incompat feature flags and remote poolset options do not match");
			errno = EINVAL;
			goto err_poolset_free;
		}
	}

	if (!can_have_rep && set->nreplicas > 1) {
		ERR("replication not supported");
		errno = ENOTSUP;
		goto err_poolset_free;
	}

	if (set->remote && util_remote_load()) {
		ERR(
			"the pool set requires a remote replica, but the '%s' library cannot be loaded",
			LIBRARY_REMOTE);
		goto err_poolset_free;
	}

	set->zeroed = 1;

	if (attr != NULL) {
		if (!util_is_zeroed(attr->poolset_uuid, POOL_HDR_UUID_LEN)) {
			memcpy(set->uuid, attr->poolset_uuid,
					POOL_HDR_UUID_LEN);
		} else {
			/* generate pool set UUID */
			ret = util_uuid_generate(set->uuid);
			if (ret < 0) {
				LOG(2, "cannot generate pool set UUID");
				goto err_poolset;
			}
		}

		/* generate UUID's for all the parts */
		for (unsigned r = 0; r < set->nreplicas; r++) {
			struct pool_replica *rep = set->replica[r];
			for (unsigned i = 0; i < rep->nhdrs; i++) {
				ret = util_uuid_generate(rep->part[i].uuid);
				if (ret < 0) {
					LOG(2,
					"cannot generate pool set part UUID");
					goto err_poolset;
				}
			}
		}

		/* overwrite UUID of the first part if given */
		if (!util_is_zeroed(attr->first_part_uuid, POOL_HDR_UUID_LEN)) {
			memcpy(set->replica[0]->part[0].uuid,
				attr->first_part_uuid, POOL_HDR_UUID_LEN);
		}
	}

	ret = util_poolset_files_local(set, minpartsize, 1);
	if (ret != 0)
		goto err_poolset;

	/* map first local replica - it has to exist prior to remote ones */
	ret = util_replica_map_local(set, 0, flags);
	if (ret != 0)
		goto err_poolset;

	/* prepare remote replicas first */
	if (set->remote) {
		for (unsigned r = 0; r < set->nreplicas; r++) {
			if (REP(set, r)->remote == NULL) {
				continue;
			}
			if (util_replica_create_remote(set, r, flags, attr) !=
					0) {
				LOG(2, "replica #%u creation failed", r);
				goto err_create;
			}
		}

		ret = util_poolset_files_remote(set, minsize, nlanes,
				1 /* create */);
		if (ret != 0)
			goto err_create;
	}

	/* prepare local replicas */
	if (remote) {
		if (util_replica_create_local(set, 0, flags, attr) != 0) {
			LOG(2, "replica #0 creation failed");
			goto err_create;
		}
	} else {
		for (unsigned r = 0; r < set->nreplicas; r++) {
			if (REP(set, r)->remote != NULL) {
				continue;
			}
			if (util_replica_create_local(set, r, flags, attr) !=
					0) {
				LOG(2, "replica #%u creation failed", r);
				goto err_create;
			}
		}
	}

	return 0;

err_create:
	oerrno = errno;
	for (unsigned r = 0; r < set->nreplicas; r++)
		util_replica_close(set, r);
	errno = oerrno;
err_poolset:
	oerrno = errno;
	util_poolset_close(set, DELETE_CREATED_PARTS);
	errno = oerrno;
	return -1;

err_poolset_free:
	oerrno = errno;
	util_poolset_free(set);
	errno = oerrno;
	return -1;
}

/*
 * util_pool_create -- create a new memory pool (set or a single file)
 *
 * On success returns 0 and a pointer to a newly allocated structure
 * containing the info of all the parts of the pool set and replicas.
 */
int
util_pool_create(struct pool_set **setp, const char *path, size_t poolsize,
	size_t minsize, size_t minpartsize, const struct pool_attr *attr,
	unsigned *nlanes, int can_have_rep)
{
	LOG(3, "setp %p path %s poolsize %zu minsize %zu minpartsize %zu "
		"attr %p nlanes %p can_have_rep %i", setp, path, poolsize,
		minsize, minpartsize, attr, nlanes, can_have_rep);

	return util_pool_create_uuids(setp, path, poolsize, minsize,
			minpartsize, attr, nlanes, can_have_rep, POOL_LOCAL);
}

/*
 * util_replica_open_local -- (internal) open a memory pool local replica
 */
static int
util_replica_open_local(struct pool_set *set, unsigned repidx, int flags)
{
	LOG(3, "set %p repidx %u flags %d", set, repidx, flags);

	int remaining_retries = 10;
	int retry_for_contiguous_addr;
	size_t mapsize;
	size_t hdrsize = (set->options & (OPTION_SINGLEHDR | OPTION_NOHDRS)) ?
			0 : Mmap_align;
	struct pool_replica *rep = set->replica[repidx];
	void *addr = NULL;

	do {
		retry_for_contiguous_addr = 0;

		/* determine a hint address for mmap() if not specified */
		if (addr == NULL)
			addr = util_map_hint(rep->resvsize, 0);
		if (addr == MAP_FAILED) {
			LOG(1, "cannot find a contiguous region of given size");
			return -1;
		}

		mapsize = rep->part[0].filesize & ~(Mmap_align - 1);

		/* map the first part and reserve space for remaining parts */
		if (util_map_part(&rep->part[0], addr, rep->resvsize, 0,
				flags, 0) != 0) {
			LOG(2, "pool mapping failed - replica #%u part #0",
				repidx);
			return -1;
		}

		VALGRIND_REGISTER_PMEM_MAPPING(rep->part[0].addr,
			rep->resvsize);
		VALGRIND_REGISTER_PMEM_FILE(rep->part[0].fd,
			rep->part[0].addr, rep->resvsize, 0);

		/* map all headers - don't care about the address */
		for (unsigned p = 0; p < rep->nhdrs; p++) {
			if (util_map_hdr(&rep->part[p], flags, 0) != 0) {
				LOG(2, "header mapping failed - part #%d", p);
				goto err;
			}
		}

		addr = (char *)rep->part[0].addr + mapsize;

		/*
		 * map the remaining parts of the usable pool space
		 * (aligned to memory mapping granularity)
		 */
		for (unsigned p = 1; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];
			size_t targetsize = mapsize +
				ALIGN_DOWN(part->filesize - hdrsize,
				part->alignment);
			if (targetsize > rep->resvsize) {
				ERR(
					"pool mapping failed - address space reservation too small");
				errno = EINVAL;
				goto err;
			}

			/* map data part */
			if (util_map_part(part, addr, 0, hdrsize,
					flags | MAP_FIXED, 0) != 0) {
				/*
				 * if we can't map the part at the address we
				 * asked for, unmap all the parts that are
				 * mapped and remap at a different address.
				 */
				if ((errno == EINVAL) &&
				    (remaining_retries > 0)) {
					LOG(2, "usable space mapping failed - "
						"part #%d - retrying", p);
					retry_for_contiguous_addr = 1;
					remaining_retries--;

					util_unmap_parts(rep, 0, p - 1);

					/* release rest of the VA reserved */
					munmap(rep->part[0].addr,
						rep->resvsize);
					break;
				}
				LOG(2, "usable space mapping failed - part #%d",
					p);
				goto err;
			}

			VALGRIND_REGISTER_PMEM_FILE(part->fd,
				part->addr, part->size, hdrsize);

			mapsize += part->size;
			addr = (char *)addr + part->size;
		}
	} while (retry_for_contiguous_addr);

	/*
	 * Initially part[0].size is the size of address space
	 * reservation for all parts from given replica. After
	 * mapping that space we need to overwrite part[0].size
	 * with its actual size to be consistent - size for each
	 * part should be the actual mapping size of this part
	 * only - it simplifies future calculations.
	 */
	rep->part[0].size = rep->part[0].filesize & ~(Mmap_align - 1);

	if (util_replica_check_map_sync(set, repidx, 1))
		goto err;

	util_replica_set_is_pmem(rep);

	if (Prefault_at_open)
		util_replica_force_page_allocation(rep);

	ASSERTeq(mapsize, rep->repsize);

	/* calculate pool size - choose the smallest replica size */
	if (rep->repsize < set->poolsize)
		set->poolsize = rep->repsize;

	LOG(3, "replica addr %p", rep->part[0].addr);

	return 0;
err:
	LOG(4, "error clean up");
	int oerrno = errno;
	if (mapsize < rep->repsize) {
		ASSERTne(rep->part[0].addr, NULL);
		ASSERTne(rep->part[0].addr, MAP_FAILED);
		munmap(rep->part[0].addr, rep->resvsize - mapsize);
	}
	for (unsigned p = 0; p < rep->nhdrs; p++)
		util_unmap_hdr(&rep->part[p]);
	for (unsigned p = 0; p < rep->nparts; p++)
		util_unmap_part(&rep->part[p]);
	errno = oerrno;
	return -1;
}

/*
 * util_replica_open_remote -- open a memory pool for remote replica
 */
int
util_replica_open_remote(struct pool_set *set, unsigned repidx, int flags)
{
	LOG(3, "set %p repidx %u flags %d", set, repidx, flags);

	struct pool_replica *rep = set->replica[repidx];

	ASSERTne(rep->remote, NULL);
	ASSERTne(rep->part, NULL);
	ASSERTeq(rep->nparts, 1);
	ASSERTeq(rep->nhdrs, 1);

	struct pool_set_part *part = rep->part;

	part->size = rep->repsize;
	ASSERT(IS_PAGE_ALIGNED(part->size));
	part->remote_hdr = Zalloc(part->size + Pagesize);
	if (!part->remote_hdr) {
		ERR("!Zalloc");
		return -1;
	}

	part->hdr = PAGE_ALIGN_UP(part->remote_hdr);
	part->addr = PAGE_ALIGN_UP(part->remote_hdr);
	part->hdrsize = POOL_HDR_SIZE;

	LOG(3, "replica #%u addr %p", repidx, rep->part[0].addr);

	return 0;
}

/*
 * util_replica_open -- open a memory pool replica
 */
int
util_replica_open(struct pool_set *set, unsigned repidx, int flags)
{
	LOG(3, "set %p repidx %u flags %d", set, repidx, flags);

	if (set->replica[repidx]->remote)
		return util_replica_open_remote(set, repidx, flags);

	return util_replica_open_local(set, repidx, flags);
}

/*
 * util_replica_set_attr -- overwrite existing replica attributes
 */
int
util_replica_set_attr(struct pool_replica *rep,
		const struct rpmem_pool_attr *rattr)
{
	LOG(3, "rep %p, rattr %p", rep, rattr);
	ASSERT(rattr != NULL || rep->nhdrs == 0);

	if (rattr != NULL && rep->nhdrs == 0) {
		ERR(
		"cannot set pool attributes for a replica without headers (with the NOHDRS option)");
		errno = EINVAL;
		return -1;
	}
	int flags = MAP_SHARED;

	/* map all headers - don't care about the address */
	for (unsigned p = 0; p < rep->nparts; p++) {
		if (util_map_hdr(&rep->part[p], flags, 0) != 0) {
			LOG(2, "header mapping failed - part #%d", p);
			goto err;
		}
	}

	for (unsigned p = 0; p < rep->nhdrs; p++) {
		ASSERTne(rattr, NULL);

		struct pool_hdr *hdrp = HDR(rep, p);
		ASSERTne(hdrp, NULL);
		util_convert2h_hdr_nocheck(hdrp);

		util_set_rpmem_attr(hdrp, rattr);

		if (hdrp == HDR(rep, 0))
			memcpy(hdrp->uuid, rattr->uuid, POOL_HDR_UUID_LEN);
		if (hdrp == HDRP(rep, 0))
			memcpy(hdrp->next_part_uuid, rattr->uuid,
					POOL_HDR_UUID_LEN);
		if (hdrp == HDRN(rep, 0))
			memcpy(hdrp->prev_part_uuid, rattr->uuid,
					POOL_HDR_UUID_LEN);

		util_convert2le_hdr(hdrp);

		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF(hdrp));

		/* store pool's header */
		util_persist_auto(rep->is_pmem, hdrp, sizeof(*hdrp));
	}

	/* unmap all headers */
	for (unsigned p = 0; p < rep->nhdrs; p++)
		util_unmap_hdr(&rep->part[p]);

	return 0;
err:
	for (unsigned p = 0; p < rep->nhdrs; p++) {
		util_unmap_hdr(&rep->part[p]);
	}
	return -1;
}

/*
 * util_get_attr_from_header -- get pool attributes from a pool header
 */
void
util_pool_hdr2attr(struct pool_attr *attr, struct pool_hdr *hdr)
{
	LOG(3, "attr %p, hdr %p", attr, hdr);
	ASSERTne(attr, NULL);
	ASSERTne(hdr, NULL);
	memset(attr, 0, sizeof(*attr));
	memcpy(attr->signature, hdr->signature, POOL_HDR_SIG_LEN);
	attr->major = hdr->major;
	attr->features.compat = hdr->features.compat;
	attr->features.incompat = hdr->features.incompat;
	attr->features.ro_compat = hdr->features.ro_compat;
	memcpy(attr->poolset_uuid, hdr->poolset_uuid, POOL_HDR_UUID_LEN);
}

/*
 * util_copy_attr_to_header -- copy pool attributes into pool header
 */
void
util_pool_attr2hdr(struct pool_hdr *hdr, const struct pool_attr *attr)
{
	LOG(3, "hdr %p, attr %p", hdr, attr);
	ASSERTne(hdr, NULL);
	ASSERTne(attr, NULL);
	memcpy(hdr->signature, attr->signature, POOL_HDR_SIG_LEN);
	hdr->major = attr->major;
	hdr->features.compat = attr->features.compat;
	hdr->features.incompat = attr->features.incompat;
	hdr->features.ro_compat = attr->features.ro_compat;
}

/*
 * util_unmap_all_hdrs -- unmap all pool set headers
 */
static void
util_unmap_all_hdrs(struct pool_set *set)
{
	LOG(3, "set %p", set);

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote == NULL) {
			for (unsigned p = 0; p < rep->nhdrs; p++)
				util_unmap_hdr(&rep->part[p]);
		} else {
			/*
			 * hdr & hdrsize were set only for util_header_check(),
			 * they will not be used any more. The memory will be
			 * freed by util_replica_close()
			 */
			rep->part[0].hdr = NULL;
			rep->part[0].hdrsize = 0;
		}
	}
}

/*
 * util_replica_check -- check headers, check UUID's, check replicas linkage
 */
static int
util_replica_check(struct pool_set *set, const struct pool_attr *attr)
{
	LOG(3, "set %p attr %p", set, attr);

	/* read shutdown state toggle from header */
	set->ignore_sds |= IGNORE_SDS(HDR(REP(set, 0), 0));

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		for (unsigned p = 0; p < rep->nhdrs; p++) {
			if (util_header_check(set, r, p, attr) != 0) {
				LOG(2, "header check failed - part #%d", p);
				return -1;
			}
			set->rdonly |= rep->part[p].rdonly;
		}

		if (memcmp(HDR(REPP(set, r), 0)->uuid,
					HDR(REP(set, r), 0)->prev_repl_uuid,
					POOL_HDR_UUID_LEN) ||
		    memcmp(HDR(REPN(set, r), 0)->uuid,
					HDR(REP(set, r), 0)->next_repl_uuid,
					POOL_HDR_UUID_LEN)) {
			ERR("wrong replica UUID");
			errno = EINVAL;
			return -1;
		}
		if (!set->ignore_sds && !rep->remote && rep->nhdrs) {
			struct shutdown_state sds;
			shutdown_state_init(&sds, NULL);
			for (unsigned p = 0; p < rep->nparts; p++) {
				if (shutdown_state_add_part(&sds,
						PART(rep, p)->path, NULL))
					return -1;
			}

			ASSERTne(rep->nhdrs, 0);
			ASSERTne(rep->nparts, 0);
			if (shutdown_state_check(&sds, &HDR(rep, 0)->sds,
					rep)) {
				LOG(2, "ADR failure detected");
				errno = EINVAL;
				return -1;
			}
			shutdown_state_set_dirty(&HDR(rep, 0)->sds,
				rep);
		}
	}
	return 0;
}

/*
 * util_pool_has_device_dax -- (internal) check if poolset has any device dax
 */
int
util_pool_has_device_dax(struct pool_set *set)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		/* either all the parts must be Device DAX or none */
		if (PART(rep, 0)->is_dev_dax)
			return 1;
	}
	return 0;
}

/*
 * util_pool_open_nocheck -- open a memory pool (set or a single file)
 *
 * This function opens a pool set without checking the header values.
 */
int
util_pool_open_nocheck(struct pool_set *set, unsigned flags)
{
	LOG(3, "set %p flags 0x%x", set, flags);

	int cow = flags & POOL_OPEN_COW;

	if (cow && util_pool_has_device_dax(set)) {
		ERR("device dax cannot be mapped privately");
		errno = ENOTSUP;
		return -1;
	}

	int mmap_flags = cow ? MAP_PRIVATE|MAP_NORESERVE : MAP_SHARED;
	int oerrno;

	ASSERTne(set, NULL);
	ASSERT(set->nreplicas > 0);

	if (flags & POOL_OPEN_CHECK_BAD_BLOCKS) {
		/* check if any bad block recovery file exists */
		int bfe = badblocks_recovery_file_exists(set);
		if (bfe > 0) {
			ERR(
				"error: a bad block recovery file exists, run 'pmempool sync --bad-blocks' utility to try to recover the pool");
			errno = EINVAL;
			return -1;
		}
		if (bfe < 0) {
			LOG(1,
				"an error occurred when checking whether recovery file exists.");
			return -1;
		}

		int bbs = badblocks_check_poolset(set, 0 /* not create */);
		if (bbs < 0) {
			LOG(1, "failed to check pool set for bad blocks");
			return -1;
		}

		if (bbs > 0) {
			if (flags & POOL_OPEN_IGNORE_BAD_BLOCKS) {
				LOG(1,
					"WARNING: pool set contains bad blocks, ignoring");
			} else {
				ERR(
					"pool set contains bad blocks and cannot be opened, run 'pmempool sync --bad-blocks' utility to try to recover the pool");
				errno = EIO;
				return -1;
			}
		}
	}

	if (set->remote && util_remote_load()) {
		ERR("the pool set requires a remote replica, "
			"but the '%s' library cannot be loaded",
			LIBRARY_REMOTE);
		return -1;
	}

	int ret = util_poolset_files_local(set, 0 /* minpartsize */, 0);
	if (ret != 0)
		goto err_poolset;

	set->rdonly = 0;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		if (util_replica_open(set, r, mmap_flags) != 0) {
			LOG(2, "replica #%u open failed", r);
			goto err_replica;
		}
	}

	if (set->remote) {
		ret = util_poolset_files_remote(set, 0, NULL, 0);
		if (ret != 0)
			goto err_replica;
	}

	util_unmap_all_hdrs(set);

	return 0;

err_replica:
	LOG(4, "error clean up");
	oerrno = errno;
	for (unsigned r = 0; r < set->nreplicas; r++)
		util_replica_close(set, r);
	errno = oerrno;
err_poolset:
	oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
	return -1;
}

/*
 * util_read_compat_features -- (internal) read compat features from the header
 */
static int
util_read_compat_features(struct pool_set *set, uint32_t *compat_features)
{
	LOG(3, "set %p pcompat_features %p", set, compat_features);

	*compat_features = 0;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];

		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; p++) {
			struct pool_set_part *part = &rep->part[p];

			if (util_part_open(part, 0, 0 /* create */)) {
				LOG(1, "!cannot open the part -- \"%s\"",
					part->path);
				/* try to open the next part */
				continue;
			}

			if (util_map_hdr(part, MAP_SHARED, 0) != 0) {
				LOG(1, "header mapping failed -- \"%s\"",
					part->path);
				util_part_fdclose(part);
				return -1;
			}

			struct pool_hdr *hdrp = part->hdr;
			*compat_features = hdrp->features.compat;

			util_unmap_hdr(part);
			util_part_fdclose(part);

			/* exit on the first successfully opened part */
			return 0;
		}
	}

	return 0;
}

/*
 * unlink_remote_replicas -- removes remote replicas from poolset
 *
 * It is necessary when COW flag is set because remote replicas
 * cannot be mapped privately
 */
static int
unlink_remote_replicas(struct pool_set *set)
{
	unsigned i = 0;
	while (i < set->nreplicas) {
		if (set->replica[i]->remote == NULL) {
			i++;
			continue;
		}

		util_replica_close(set, i);
		int ret = util_replica_close_remote(set->replica[i], i,
				DO_NOT_DELETE_PARTS);
		if (ret != 0)
			return ret;

		size_t size = sizeof(set->replica[i]) *
			(set->nreplicas - i - 1);
		memmove(&set->replica[i], &set->replica[i + 1], size);
		set->nreplicas--;
	}

	set->remote = 0;
	return 0;
}

/*
 * util_pool_open -- open a memory pool (set or a single file)
 *
 * This routine does all the work, but takes a rdonly flag so internal
 * calls can map a read-only pool if required.
 */
int
util_pool_open(struct pool_set **setp, const char *path, size_t minpartsize,
	const struct pool_attr *attr, unsigned *nlanes, void *addr,
	unsigned flags)
{
	LOG(3, "setp %p path %s minpartsize %zu attr %p nlanes %p "
		"addr %p flags 0x%x ", setp, path, minpartsize, attr, nlanes,
		addr, flags);

	int cow = flags & POOL_OPEN_COW;
	int mmap_flags = cow ? MAP_PRIVATE|MAP_NORESERVE : MAP_SHARED;
	int oerrno;

	/* do not check minsize */
	int ret = util_poolset_create_set(setp, path, 0, 0,
						flags & POOL_OPEN_IGNORE_SDS);
	if (ret < 0) {
		LOG(2, "cannot open pool set -- '%s'", path);
		return -1;
	}

	if ((*setp)->replica[0]->nparts == 0) {
		errno = ENOENT;
		ERR("!no parts in replicas");
		goto err_poolset_free;
	}

	if (cow && (*setp)->replica[0]->part[0].is_dev_dax) {
		ERR("device dax cannot be mapped privately");
		errno = ENOTSUP;
		goto err_poolset_free;
	}

	struct pool_set *set = *setp;

	ASSERT(set->nreplicas > 0);

	uint32_t compat_features;

	if (util_read_compat_features(set, &compat_features)) {
		LOG(1, "reading compat features failed");
		goto err_poolset_free;
	}

	if (compat_features & POOL_FEAT_CHECK_BAD_BLOCKS) {
		/* check if any bad block recovery file exists */
		int bfe = badblocks_recovery_file_exists(set);
		if (bfe > 0) {
			ERR(
				"error: a bad block recovery file exists, run 'pmempool sync --bad-blocks' utility to try to recover the pool");
			errno = EINVAL;
			goto err_poolset_free;
		}

		if (bfe < 0) {
			LOG(1,
				"an error occurred when checking whether recovery file exists.");
			goto err_poolset_free;
		}

		int bbs = badblocks_check_poolset(set, 0 /* not create */);
		if (bbs < 0) {
			LOG(1,
				"failed to check pool set for bad blocks -- '%s'",
				path);
			goto err_poolset_free;
		}

		if (bbs > 0) {
			if (flags & POOL_OPEN_IGNORE_BAD_BLOCKS) {
				LOG(1,
					"WARNING: pool set contains bad blocks, ignoring -- '%s'",
					path);
			} else {
				ERR(
					"pool set contains bad blocks and cannot be opened, run 'pmempool sync --bad-blocks' utility to try to recover the pool -- '%s'",
					path);
				errno = EIO;
				goto err_poolset_free;
			}
		}
	}

	if (set->remote && util_remote_load()) {
		ERR(
			"the pool set requires a remote replica, but the '%s' library cannot be loaded",
			LIBRARY_REMOTE);
		goto err_poolset_free;
	}

	ret = util_poolset_files_local(set, minpartsize, 0);
	if (ret != 0)
		goto err_poolset;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		if (util_replica_open(set, r, mmap_flags) != 0) {
			LOG(2, "replica #%u open failed", r);
			goto err_replica;
		}
	}

	if (set->remote) {
		/* do not check minsize */
		ret = util_poolset_files_remote(set, 0, nlanes, 0);
		if (ret != 0)
			goto err_replica;
	}

	/* check headers, check UUID's, check replicas linkage */
	if (attr != NULL && util_replica_check(set, attr))
		goto err_replica;

	/* unmap all headers */
	util_unmap_all_hdrs(set);

	/* remove all remote replicas from poolset when cow */
	if (cow && set->remote) {
		ret = unlink_remote_replicas(set);
		if (ret != 0)
			goto err_replica;
	}

	return 0;

err_replica:
	LOG(4, "error clean up");
	oerrno = errno;
	for (unsigned r = 0; r < set->nreplicas; r++)
		util_replica_close(set, r);
	errno = oerrno;
err_poolset:
	oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
	return -1;

err_poolset_free:
	oerrno = errno;
	util_poolset_free(*setp);
	errno = oerrno;
	return -1;
}

/*
 * util_pool_open_remote -- open a remote pool set file
 *
 * This routine does all the work, but takes a rdonly flag so internal
 * calls can map a read-only pool if required.
 */
int
util_pool_open_remote(struct pool_set **setp, const char *path, int cow,
	size_t minpartsize, struct rpmem_pool_attr *rattr)
{
	LOG(3, "setp %p path %s cow %d minpartsize %zu rattr %p",
		setp, path, cow, minpartsize, rattr);

	int flags = cow ? MAP_PRIVATE|MAP_NORESERVE : MAP_SHARED;
	int oerrno;

	/* do not check minsize */
	int ret = util_poolset_create_set(setp, path, 0, 0, 0);
	if (ret < 0) {
		LOG(2, "cannot open pool set -- '%s'", path);
		return -1;
	}

	if (cow && (*setp)->replica[0]->part[0].is_dev_dax) {
		ERR("device dax cannot be mapped privately");
		errno = ENOTSUP;
		return -1;
	}

	struct pool_set *set = *setp;

	if (set->nreplicas > 1) {
		LOG(2, "remote pool set cannot have replicas");
		goto err_poolset;
	}

	uint32_t compat_features;

	if (util_read_compat_features(set, &compat_features)) {
		LOG(1, "reading compat features failed");
		goto err_poolset;
	}

	if (compat_features & POOL_FEAT_CHECK_BAD_BLOCKS) {
		/* check if there are any bad blocks */
		int bbs = badblocks_check_poolset(set, 0 /* not create */);
		if (bbs < 0) {
			LOG(1,
				"failed to check the remote replica for bad blocks -- '%s'",
				path);
			goto err_poolset;
		}

		if (bbs > 0) {
			ERR(
				"remote replica contains bad blocks and cannot be opened, run 'pmempool sync --bad-blocks' utility to recreate it -- '%s'",
				path);
			errno = EIO;
			goto err_poolset;
		}
	}

	ret = util_poolset_files_local(set, minpartsize, 0);
	if (ret != 0)
		goto err_poolset;

	if (util_replica_open(set, 0, flags) != 0) {
		LOG(2, "replica open failed");
		goto err_replica;
	}

	struct pool_replica *rep = set->replica[0];

	set->rdonly |= rep->part[0].rdonly;

	/* check headers, check UUID's, check replicas linkage */
	for (unsigned p = 0; p < rep->nhdrs; p++) {
		if (util_header_check_remote(set, p) != 0) {
			LOG(2, "header check failed - part #%d", p);
			goto err_replica;
		}
		set->rdonly |= rep->part[p].rdonly;
	}

	if (rep->nhdrs > 0) {
		/* header exists, copy pool attributes */
		struct pool_hdr *hdr = rep->part[0].hdr;
		util_get_rpmem_attr(rattr, hdr);
	} else {
		/* header does not exist, zero pool attributes */
		memset(rattr, 0, sizeof(*rattr));
	}

	/* unmap all headers */
	for (unsigned p = 0; p < rep->nhdrs; p++)
		util_unmap_hdr(&rep->part[p]);

	return 0;

err_replica:
	LOG(4, "error clean up");
	oerrno = errno;
	util_replica_close(set, 0);
	errno = oerrno;
err_poolset:
	oerrno = errno;
	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	errno = oerrno;
	return -1;
}

/*
 * util_is_poolset_file -- check if specified file is a poolset file
 *
 * Return value:
 * -1 - error
 *  0 - not a poolset
 *  1 - is a poolset
 */
int
util_is_poolset_file(const char *path)
{
	enum file_type type = util_file_get_type(path);
	if (type < 0)
		return -1;

	if (type == TYPE_DEVDAX)
		return 0;

	int fd = util_file_open(path, NULL, 0, O_RDONLY);
	if (fd < 0)
		return -1;

	int ret = 0;
	ssize_t sret;
	char signature[POOLSET_HDR_SIG_LEN];
	size_t rd = 0;
	do {
		sret = util_read(fd, &signature[rd], sizeof(signature) - rd);
		if (sret > 0)
			rd += (size_t)sret;
	} while (sret > 0);
	if (sret < 0) {
		ERR("!read");
		ret = -1;
		goto out;
	} else if (rd != sizeof(signature)) {
		ret = 0;
		goto out;
	}

	if (memcmp(signature, POOLSET_HDR_SIG, POOLSET_HDR_SIG_LEN) == 0)
		ret = 1;
out:
	os_close(fd);
	return ret;
}
/*
 * util_poolset_foreach_part_struct -- walk through all poolset file parts
 *                                  of the given set
 *
 * Stops processing if callback returns non-zero value.
 * The value returned by callback is returned to the caller.
 */
int
util_poolset_foreach_part_struct(struct pool_set *set,
	int (*callback)(struct part_file *pf, void *arg), void *arg)
{
	LOG(3, "set %p callback %p arg %p", set, callback, arg);

	ASSERTne(callback, NULL);

	int ret;

	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct part_file cbdata;
		if (set->replica[r]->remote) {
			cbdata.is_remote = 1;
			cbdata.remote = set->replica[r]->remote;
			cbdata.part = NULL;
			ret = (*callback)(&cbdata, arg);
			if (ret)
				return ret;
		} else {
			cbdata.is_remote = 0;
			cbdata.remote = NULL;
			for (unsigned p = 0; p < set->replica[r]->nparts; p++) {
				cbdata.part = &set->replica[r]->part[p];
				ret = (*callback)(&cbdata, arg);
				if (ret)
					return ret;
			}
		}
	}

	return 0;
}

/*
 * util_poolset_foreach_part -- walk through all poolset file parts
 *
 * Stops processing if callback returns non-zero value.
 * The value returned by callback is returned to the caller.
 *
 * Return value:
 *  0 - all part files have been processed
 * -1 - parsing poolset file error
 */
int
util_poolset_foreach_part(const char *path,
	int (*callback)(struct part_file *pf, void *arg), void *arg)
{
	LOG(3, "path %s callback %p arg %p", path, callback, arg);

	ASSERTne(callback, NULL);

	int fd = os_open(path, O_RDONLY);
	if (fd < 0) {
		ERR("!open: path \"%s\"", path);
		return -1;
	}

	struct pool_set *set;
	int ret = util_poolset_parse(&set, path, fd);
	if (ret) {
		ERR("util_poolset_parse failed -- '%s'", path);
		ret = -1;
		goto err_close;
	}

	ret = util_poolset_foreach_part_struct(set, callback, arg);

	/*
	 * Make sure callback does not return -1,
	 * because this value is reserved for parsing
	 * error.
	 */
	ASSERTne(ret, -1);
	util_poolset_free(set);

err_close:
	os_close(fd);
	return ret;
}

/*
 * util_poolset_size -- get size of poolset, returns 0 on error
 */
size_t
util_poolset_size(const char *path)
{
	int fd = os_open(path, O_RDONLY);
	if (fd < 0)
		return 0;

	size_t size = 0;
	struct pool_set *set;
	if (util_poolset_parse(&set, path, fd))
		goto err_close;

	size = set->poolsize;

	util_poolset_free(set);
err_close:
	os_close(fd);
	return size;
}

/*
 * util_replica_fdclose -- close all parts of given replica
 */
void
util_replica_fdclose(struct pool_replica *rep)
{
	for (unsigned p = 0; p < rep->nparts; p++) {
		struct pool_set_part *part = &rep->part[p];
		util_part_fdclose(part);
	}
}

/*
 * util_replica_deep_common -- performs common calculations
 * on all parts from replica to define intersection ranges
 * for final flushing operations that take place in
 * os_part_deep_common function.
 */
int
util_replica_deep_common(const void *addr, size_t len, struct pool_set *set,
				unsigned replica_id, int flush)
{
	LOG(3, "addr %p len %zu set %p replica_id %u flush %d",
		addr, len, set, replica_id, flush);

	struct pool_replica *rep = set->replica[replica_id];
	uintptr_t rep_start = (uintptr_t)rep->part[0].addr;
	uintptr_t rep_end = rep_start + rep->repsize;
	uintptr_t start = (uintptr_t)addr;
	uintptr_t end = start + len;

	ASSERT(start >= rep_start);
	ASSERT(end <= rep_end);

	for (unsigned p = 0; p < rep->nparts; p++) {
		struct pool_set_part *part = &rep->part[p];
		uintptr_t part_start = (uintptr_t)part->addr;
		uintptr_t part_end = part_start + part->size;
		/* init intersection start and end addresses */
		uintptr_t range_start = start;
		uintptr_t range_end = end;

		if (part_start > end || part_end < start)
			continue;
		/* recalculate intersection addresses */
		if (part_start > start)
			range_start = part_start;
		if (part_end < end)
			range_end = part_end;
		size_t range_len = range_end - range_start;

		LOG(15, "perform deep flushing for replica %u "
			"part %p, addr %p, len %lu",
			replica_id, part, (void *)range_start, range_len);
		if (os_part_deep_common(rep, p, (void *)range_start,
				range_len, flush)) {
			LOG(1, "os_part_deep_common(%p, %p, %lu)",
				part, (void *)range_start, range_len);
			return -1;
		}
	}
	return 0;
}

/*
 * util_replica_deep_persist -- wrapper for util_replica_deep_common
 * Calling the target precedes initialization of function that
 * partly defines way of deep replica flushing.
 */
int
util_replica_deep_persist(const void *addr, size_t len, struct pool_set *set,
				unsigned replica_id)
{
	LOG(3, "addr %p len %zu set %p replica_id %u",
		addr, len, set, replica_id);

	int flush = 1;
	return util_replica_deep_common(addr, len, set, replica_id, flush);
}

/*
 * util_replica_deep_drain -- wrapper for util_replica_deep_common
 * Calling the target precedes initialization of function that
 * partly defines way of deep replica flushing.
 */
int
util_replica_deep_drain(const void *addr, size_t len, struct pool_set *set,
				unsigned replica_id)
{
	LOG(3, "addr %p len %zu set %p replica_id %u",
		addr, len, set, replica_id);

	int flush = 0;
	return util_replica_deep_common(addr, len, set, replica_id, flush);
}
