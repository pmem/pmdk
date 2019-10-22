/*
 * Copyright 2019, Intel Corporation
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
 * map_posix.c -- pmem2_map (POSIX)
 */

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "valgrind_internal.h"

#include "libpmem2.h"
#include "out.h"
#include "config.h"
#include "map.h"
#include "pmem2_utils.h"

#ifndef MAP_SYNC
#define MAP_SYNC 0x80000
#endif

#ifndef MAP_SHARED_VALIDATE
#define MAP_SHARED_VALIDATE 0x03
#endif

#define MEGABYTE ((uintptr_t)1 << 20)
#define GIGABYTE ((uintptr_t)1 << 30)

/*
 * map_get_alignment -- (internal) choose the desired mapping alignment
 *
 * The smallest supported alignment is 2 megabytes because of the object
 * alignment requirements. Changing this value to 4 kilobytes constitutes a
 * layout change.
 *
 * Use 1GB page alignment only if the mapping length is at least
 * twice as big as the page size.
 */
static inline size_t
map_get_alignment(size_t len, size_t req_align)
{
	size_t align = 2 * MEGABYTE;
	if (req_align)
		align = req_align;
	else if (len >= 2 * GIGABYTE)
		align = GIGABYTE;

	return align;
}

/*
 * map_hint -- (internal) determine hint address for mmap()
 *
 * ALSR in 64-bit Linux kernel uses 28-bit of randomness for mmap
 * (bit positions 12-39), which means the base mapping address is randomized
 * within [0..1024GB] range, with 4KB granularity.  Assuming additional
 * 1GB alignment, it results in 1024 possible locations.
 */
static char *
map_hint(size_t len, size_t alignment, int *ret)
{
	char *hint_addr = MAP_FAILED;
	ret = PMEM2_E_OK;

	/*
	 * Create dummy mapping to find an unused region of given size.
	 * Request for increased size for later address alignment.
	 * Use MAP_PRIVATE with read-only access to simulate
	 * zero cost for overcommit accounting.  Note: MAP_NORESERVE
	 * flag is ignored if overcommit is disabled (mode 2).
	 */
	char *addr = mmap(NULL, len + alignment, PROT_READ,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (addr == MAP_FAILED) {
		ERR("!mmap MAP_ANONYMOUS");
	} else {
		LOG(4, "system choice %p", addr);
		hint_addr = (char *)roundup((uintptr_t)addr, alignment);
		munmap(addr, len + alignment);
	}
	LOG(4, "hint %p", hint_addr);

	return hint_addr;
}

/*
 * util_map_sync -- memory map given file into memory, if MAP_SHARED flag is
 * provided it attempts to use MAP_SYNC flag. Otherwise it fallbacks to
 * mmap(2).
 */
static void *
util_map_sync(void *addr, size_t len, int proto, int flags, int fd,
	os_off_t offset, int *map_sync)
{
	LOG(15, "addr %p len %zu proto %x flags %x fd %d offset %ld "
		"map_sync %p", addr, len, proto, flags, fd, offset, map_sync);

	if (map_sync)
		*map_sync = 0;

	/* if map_sync is NULL do not even try to mmap with MAP_SYNC flag */
	if (!map_sync || flags & MAP_PRIVATE)
		return mmap(addr, len, proto, flags, fd, offset);

	/* MAP_SHARED */
	void *ret = mmap(addr, len, proto,
			flags | MAP_SHARED_VALIDATE | MAP_SYNC,
			fd, offset);
	if (ret != MAP_FAILED) {
		LOG(4, "mmap with MAP_SYNC succeeded");
		*map_sync = 1;
		return ret;
	}

	if (errno == EINVAL || errno == ENOTSUP) {
		LOG(4, "mmap with MAP_SYNC not supported");
		return mmap(addr, len, proto, flags, fd, offset);
	}

	/* other error */
	return MAP_FAILED;
}

/*
 * util_map -- memory map a file
 */
static void *
util_map(int fd, void *hint, os_off_t off, size_t len, int flags, int rdonly,
	int *map_sync)
{
	void *base;

	int proto = rdonly ? PROT_READ : PROT_READ|PROT_WRITE;
	base = util_map_sync(hint, len, proto, flags, fd, off, map_sync);
	if (base == MAP_FAILED) {
		ERR("!mmap %zu bytes", len);
		return NULL;
	}

	LOG(3, "mapped at %p", base);

	return base;
}

/*
 * unmap -- (internal) unmap a memory range
 */
static int
unmap(void *addr, size_t len)
{
/*
 * XXX Workaround for https://bugs.freebsd.org/bugzilla/show_bug.cgi?id=169608
 */
#ifdef __FreeBSD__
	if (!IS_PAGE_ALIGNED((uintptr_t)addr)) {
		ERR("!munmap");
		return PMEM2_E_INVALID_ARG;
	}
#endif
	int retval = munmap(addr, len);
	if (retval < 0) {
		ERR("!munmap");
		return PMEM2_E_INVALID_ARG;
	}

	return PMEM2_E_OK;
}

/*
 * pmem2_map -- map memory according to provided config
 */
int
pmem2_map(const struct pmem2_config *cfg, struct pmem2_map **map_ptr)
{
	LOG(3, "cfg %p map_ptr %p", cfg, map_ptr);
	int ret = PMEM2_E_OK;
	struct pmem2_map *map;
	ssize_t file_len;

	os_off_t off = (os_off_t)cfg->offset;
	ASSERTeq((size_t)off, cfg->offset);
	enum file_type file_type = util_fd_get_type(cfg->fd);

	/* map input and output variables */
	int map_sync;
	int flags = MAP_SHARED;
	int rdonly = 0; /* PROT_READ | PROT_WRITE */
	void *addr;

	if (file_type == OTHER_ERROR)
		return PMEM2_E_UNKNOWN_FILETYPE;

	ASSERT(file_type == TYPE_NORMAL || file_type == TYPE_DEVDAX);

	file_len = util_fd_get_size(cfg->fd);
	if (file_len < 0) {
		ERR("unable to read file size");
		return PMEM2_E_INV_FSIZE;
	}

	/* overflow check */
	const size_t end = cfg->offset + cfg->length;
	if (end < cfg->offset)
		return PMEM2_E_MAP_RANGE;

	/* validate mapping fit into the file */
	if (end > (size_t)file_len)
		return PMEM2_E_MAP_RANGE;

	/* without user-provided length map to the end of the file */
	size_t length = cfg->length;
	if (!length)
		length = (size_t)file_len - cfg->offset;

	const size_t alignment = map_get_alignment(length, cfg->alignment);

	void *hint = map_hint(length, alignment, &ret);
	if (ret != PMEM2_E_OK) {
		LOG(1, "cannot find a contiguous region of given size");
		return ret;
	}

	addr = util_map(cfg->fd, hint, off, length, flags, rdonly, &map_sync);
	if (addr == NULL && errno == EACCES) {
		rdonly = 1; /* PROT_READ */
		addr = util_map(cfg->fd, hint, off, length, flags, rdonly,
				&map_sync);
	}

	if (addr == NULL)
		return PMEM2_E_MAP_FAILED;

	/* prepare pmem2_map structure */
	map = (struct pmem2_map *)pmem2_malloc(sizeof(*map), &ret);
	if (!map)
		goto err_unmap;

	map->addr = addr;
	map->file_type = file_type;
	map->map_sync = map_sync;
	map->length = length;
	*map_ptr = map;

	VALGRIND_REGISTER_PMEM_MAPPING(addr, length);
	VALGRIND_REGISTER_PMEM_FILE(cfg->fd, addr, length, 0);

	return ret;

err_unmap:
	unmap(addr, length);
	return ret;
}

/*
 * pmem2_unmap -- unmap the specified region
 */
int
pmem2_unmap(struct pmem2_map **map_ptr)
{
	LOG(3, "map_ptr %p", map_ptr);

	int ret = PMEM2_E_OK;
	struct pmem2_map *map = *map_ptr;
	struct pmem2_config *cfg = map->cfg;

	VALGRIND_REMOVE_PMEM_MAPPING(map->addr, cfg->length);

	if (util_unmap(map->addr, cfg->length))
		ERR("!util_unmap");

	pmem2_config_delete(&cfg);

	Free(map);
	map = NULL;

	return ret;
}
