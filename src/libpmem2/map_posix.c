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
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>

#include "valgrind_internal.h"

#include "libpmem2.h"
#include "alloc.h"
#include "out.h"
#include "file.h"
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
 * get_map_alignment -- (internal) choose the desired mapping alignment
 *
 * The smallest supported alignment is 2 megabytes because of the object
 * alignment requirements. Changing this value to 4 kilobytes constitutes a
 * layout change.
 *
 * Use 1GB page alignment only if the mapping length is at least
 * twice as big as the page size.
 */
static inline size_t
get_map_alignment(size_t len, size_t req_align)
{
	size_t align = 2 * MEGABYTE;
	if (req_align)
		align = req_align;
	else if (len >= 2 * GIGABYTE)
		align = GIGABYTE;

	return align;
}

/*
 * get_map_hint -- (internal) determine hint address for mmap()
 *
 * ALSR in 64-bit Linux kernel uses 28-bit of randomness for mmap
 * (bit positions 12-39), which means the base mapping address is randomized
 * within [0..1024GB] range, with 4KB granularity.  Assuming additional
 * 1GB alignment, it results in 1024 possible locations.
 */
static int
get_map_hint(size_t len, size_t alignment, void **hint)
{
	ASSERTne(*hint, NULL);

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
		return PMEM2_E_ERRNO;
	}

	LOG(4, "system choice %p", addr);
	*hint = (void *)roundup((uintptr_t)addr, alignment);
	munmap(addr, len + alignment);
	LOG(4, "hint %p", hint);

	return PMEM2_E_OK;
}

/*
 * file_map -- (internal) memory map given file into memory
 * If (flags & MAP_PRIVATE) it uses just mmap. Otherwise, it tries to mmap with
 * (flags | MAP_SHARED_VALIDATE | MAP_SYNC) which allows flushing from the
 * user-space. If MAP_SYNC fails and the user did not specify it by himself it
 * falls back to the mmap with user-provided flags.
 */
static int
file_map(void *hint, size_t len, int proto, int flags, int fd, off_t offset,
		bool *map_sync, void **base)
{
	LOG(15, "hint %p len %zu proto %x flags %x fd %d offset %ld "
			"map_sync %p", hint, len, proto, flags, fd, offset,
			map_sync);

	ASSERTne(map_sync, NULL);
	ASSERTne(*base, NULL);

	/*
	 * MAP_PRIVATE is mutually exclusive with MAP_SHARED_VALIDATE which is
	 * required for MAP_SYNC
	 */
	if (flags & MAP_PRIVATE) {
		*base = mmap(hint, len, proto, flags, fd, offset);
		if (*base != MAP_FAILED) {
			*map_sync = false;
			return PMEM2_E_OK;
		}

		return PMEM2_E_ERRNO;
	}

	/* try to mmap with MAP_SYNC flag */
	const int flags_with_sync = flags | MAP_SHARED_VALIDATE | MAP_SYNC;
	*base = mmap(hint, len, proto, flags_with_sync, fd, offset);
	if (*base != MAP_FAILED) {
		LOG(4, "mmap with MAP_SYNC succeeded");
		*map_sync = true;
		return PMEM2_E_OK;
	}

	/* try to mmap without MAP_SYNC flag */
	if ((flags_with_sync != flags) &&
			(errno == EINVAL || errno == ENOTSUP)) {
		LOG(4, "mmap with MAP_SYNC not supported");
		*base = mmap(hint, len, proto, flags, fd, offset);
		if (*base != MAP_FAILED) {
			*map_sync = false;
			return PMEM2_E_OK;
		}
	}

	return PMEM2_E_ERRNO;
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
		return PMEM2_E_ERRNO;
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
	size_t file_len;

	if (cfg->fd == INVALID_FD)
		return PMEM2_E_INVALID_FILE_HANDLE;

	os_off_t off = (os_off_t)cfg->offset;
	ASSERTeq((size_t)off, cfg->offset);

	/* get file size */
	ret = pmem2_config_get_file_size(cfg, &file_len);
	if (ret)
		return ret;

	/* get file type */
	enum pmem2_file_type file_type;
	os_stat_t st;
	if (os_fstat(cfg->fd, &st))
		return PMEM2_E_ERRNO;
	ret = pmem2_get_type_from_stat(&st, &file_type);
	if (ret)
		return ret;

	/* map input and output variables */
	bool map_sync;
	int flags = MAP_SHARED;
	int proto = PROT_READ | PROT_WRITE;
	void *addr;

	if (file_type == PMEM2_FTYPE_DIR)
		return PMEM2_E_INVALID_FILE_TYPE;

	ASSERT(file_type == PMEM2_FTYPE_REG || file_type == PMEM2_FTYPE_DEVDAX);

	size_t length;
	ret = pmem2_verify_range(cfg, file_len, &length);
	if (ret)
		return ret;

	const size_t alignment = get_map_alignment(length, cfg->alignment);

	/* find a hint for the mapping */
	void *hint = NULL;
	ret = get_map_hint(length, alignment, &hint);
	if (ret != PMEM2_E_OK) {
		LOG(1, "cannot find a contiguous region of given size");
		return ret;
	}
	ASSERTne(hint, NULL);

	ret = file_map(hint, length, proto, flags, cfg->fd, off, &map_sync,
			&addr);
	if (ret == -EACCES) {
		proto = PROT_READ;
		ret = file_map(hint, length, proto, flags, cfg->fd, off,
				&map_sync, &addr);
	}

	if (ret)
		return ret;

	LOG(3, "mapped at %p", addr);

	/* prepare pmem2_map structure */
	map = (struct pmem2_map *)pmem2_malloc(sizeof(*map), &ret);
	if (!map) {
		unmap(addr, length);
		return ret;
	}

	map->addr = addr;
	/* XXX require eADR detection to set PMEM2_GRANULARITY_BYTE */
	map->effective_granularity = map_sync ? PMEM2_GRANULARITY_CACHE_LINE :
			PMEM2_GRANULARITY_PAGE;
	map->length = length;
	*map_ptr = map;

	VALGRIND_REGISTER_PMEM_MAPPING(map->addr, map->length);
	VALGRIND_REGISTER_PMEM_FILE(cfg->fd, map->addr, map->length, 0);

	return ret;
}

/*
 * pmem2_unmap -- unmap the specified mapping
 */
int
pmem2_unmap(struct pmem2_map **map_ptr)
{
	LOG(3, "map_ptr %p", map_ptr);

	int ret = PMEM2_E_OK;
	struct pmem2_map *map = *map_ptr;

	ret = unmap(map->addr, map->length);
	if (ret)
		return ret;

	VALGRIND_REMOVE_PMEM_MAPPING(map->addr, map->length);

	Free(map);
	*map_ptr = NULL;

	return ret;
}
