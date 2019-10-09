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
 * pmem2.c -- pmem2 entry points for libpmem2
 */

#include <string.h>

#include "file.h"
#include "valgrind_internal.h"

#include "libpmem2.h"
#include "pmem2.h"
#include "pmem.h"

struct pmem2_config {
	int fd;
	size_t length;
	size_t offset;
};

struct pmem2_map {
	struct pmem2_config *cfg;
	void *addr;
};

/*
 * pmem2_map -- XXX
 */
int
pmem2_map(struct pmem2_config *cfg, struct pmem2_map **mapp)
{
	/* XXX LOG(3, "cfg %p map %p", cfg, map); */

	int file_type = util_fd_get_type(cfg->fd);

	if (file_type == OTHER_ERROR)
		return PMEM2_E_UNKNOWN_FILETYPE;

	ssize_t actual_len = util_fd_get_size(cfg->fd);
	if (actual_len < 0) {
		/* XXX ERR("unable to read file size"); placeholder */
		return PMEM2_E_INV_FSIZE;
	}
	/*
	 * XXX I'm not sure if we want to hold this restriction
	 * if (len != 0 && len != (size_t)actual_len) {
	 * 	ERR("Device DAX length must be either 0 or "
	 * 		"the exact size of the device: %zu",
	 * 		actual_len);
	 * 	errno = EINVAL;
	 * 	return NULL;
	 * }
	 */
	
	if (cfg->offset + cfg->length > (size_t)actual_len)
		return PMEM2_E_MAP_RANGE;

	/* XXX verify offset is in range durring config_set_offset? */
	os_off_t off = (os_off_t)cfg->offset;

	void *addr = pmem_map_register(cfg->fd, off, cfg->length, file_type == TYPE_DEVDAX);
	if (addr == NULL)
		return PMEM2_E_MAP_FAILED;

	struct pmem2_map *map = (struct pmem2_map *)calloc(1, sizeof(*map));
	map->cfg = (struct pmem2_config *)calloc(1, sizeof(*map->cfg));
	memcpy(map->cfg, cfg, sizeof(*cfg));
	map->addr = addr;
	*mapp = map;

	VALGRIND_REGISTER_PMEM_MAPPING(addr, cfg->length);
	VALGRIND_REGISTER_PMEM_FILE(cfg->fd, addr, cfg->length, 0);

	return PMEM2_E_OK;
}
