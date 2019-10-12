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
 * pmem2_posix.c -- pmem2 entry points for libpmem2 (POSIX)
 */

#include <errno.h>
#include <string.h>
#include <sys/mman.h>

#include "alloc.h"
#include "file.h"
#include "mmap.h"
#include "valgrind_internal.h"

#include "libpmem2.h"
#include "pmem2.h"
#include "pmem2_utils.h"

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
	int file_type = util_fd_get_type(cfg->fd);

	/* map input and output variables */
	int map_sync;
	int flags = MAP_SHARED;
	int rdonly = 0; /* PROT_READ | PROT_WRITE */
	size_t req_align = 0; /* no required alignment */
	void *addr;

	if (file_type == OTHER_ERROR)
		return PMEM2_E_UNKNOWN_FILETYPE;

	file_len = util_fd_get_size(cfg->fd);
	if (file_len < 0) {
		ERR("unable to read file size");
		return PMEM2_E_INV_FSIZE;
	}

	/* validate mapping fit into the file */
	if (cfg->offset + cfg->length > (size_t)file_len)
		return PMEM2_E_MAP_RANGE;

	addr = util_map(cfg->fd, off, cfg->length, flags, rdonly, req_align,
			&map_sync);
	if (addr == NULL)
		return PMEM2_E_MAP_FAILED;

	/* prepare pmem2_map structure */
	map = (struct pmem2_map *)pmem2_zalloc(sizeof(*map), &ret);
	if (!map)
		goto err_unmap;
	map->cfg = (struct pmem2_config *)pmem2_zalloc(sizeof(*map->cfg), &ret);
	if (!map->cfg) {
		goto err_free_map;
	}

	memcpy(map->cfg, cfg, sizeof(*cfg));
	map->addr = addr;
	map->sync = map_sync;
	*map_ptr = map;

	VALGRIND_REGISTER_PMEM_MAPPING(addr, cfg->length);
	VALGRIND_REGISTER_PMEM_FILE(cfg->fd, addr, cfg->length, 0);

	return ret;

err_free_map:
	free(map);
err_unmap:
	util_unmap(addr, cfg->length);
	return ret;
}
