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
 * map_windows.c -- pmem2_map (Windows)
 */

#include <stdbool.h>

#include "libpmem2.h"
#include "out.h"
#include "config.h"
#include "map.h"
#include "pmem2_utils.h"
#include "alloc.h"
#include "util.h"

#define HIDWORD(x) ((DWORD)((x) >> 32))
#define LODWORD(x) ((DWORD)((x) & 0xFFFFFFFF))

/*
 * create_mapping -- creates file mapping object for a file
 */
HANDLE
create_mapping(HANDLE hfile, size_t offset, size_t length, DWORD protect,
		unsigned long *err)
{
	size_t max_size = length + offset;
	SetLastError(0);
	HANDLE mh = CreateFileMapping(hfile,
		NULL, /* security attributes */
		protect,
		HIDWORD(max_size),
		LODWORD(max_size),
		NULL);

	*err = GetLastError();
	if (!mh) {
		ERR("!!CreateFileMapping");
		return NULL;
	}

	if (*err == ERROR_ALREADY_EXISTS) {
		ERR("!!CreateFileMapping");
		CloseHandle(mh);
		return NULL;
	}

	/* if the handle is valid the last error is undefined */
	*err = 0;
	return mh;
}

/*
 * pmem2_map -- map memory according to provided config
 */
int
pmem2_map(const struct pmem2_config *cfg, struct pmem2_map **map_ptr)
{
	LOG(3, "cfg %p map_ptr %p", cfg, map_ptr);

	int ret = PMEM2_E_OK;
	unsigned long err = 0;
	size_t file_size;

	if (cfg->handle == INVALID_HANDLE_VALUE) {
		ERR("the provided file handle is invalid");
		return PMEM2_E_INVALID_FILE_HANDLE;
	}

	ret = pmem2_config_get_file_size(cfg, &file_size);
	if (ret)
		return ret;

	size_t length;
	ret = pmem2_get_length(cfg, file_size, &length);
	if (ret)
		return ret;

	/* create a file mapping handle */
	DWORD access = FILE_MAP_ALL_ACCESS;
	HANDLE mh = create_mapping(cfg->handle, cfg->offset, length,
			PAGE_READWRITE, &err);
	if (err == ERROR_ACCESS_DENIED) {
		mh = create_mapping(cfg->handle, cfg->offset, length,
				PAGE_READONLY, &err);
		access = FILE_MAP_READ;
	}

	if (!mh) {
		if (err == ERROR_ALREADY_EXISTS)
			return PMEM2_E_MAPPING_EXISTS;

		return pmem2_lasterror_to_err();
	}

	/* obtain a pointer to the mapping view */
	void *base = MapViewOfFileEx(mh,
		access,
		HIDWORD(cfg->offset),
		LODWORD(cfg->offset),
		length,
		NULL); /* hint address */

	if (base == NULL) {
		ERR("!!MapViewOfFileEx");
		ret = pmem2_lasterror_to_err();
		goto err_close_mapping_handle;
	}

	if (!CloseHandle(mh)) {
		ERR("!!CloseHandle");
		ret = pmem2_lasterror_to_err();
		goto err_unmap_base;
	}

	/* prepare pmem2_map structure */
	struct pmem2_map *map;
	map = (struct pmem2_map *)pmem2_malloc(sizeof(*map), &ret);
	if (!map)
		goto err_unmap_base;

	map->addr = base;
	map->length = length;
	/* XXX require eADR detection to set PMEM2_GRANULARITY_BYTE */
	map->effective_granularity = PMEM2_GRANULARITY_PAGE;
	/* return a pointer to the pmem2_map structure */
	*map_ptr = map;

	return ret;

err_unmap_base:
	UnmapViewOfFile(base);
	return ret;

err_close_mapping_handle:
	CloseHandle(mh);
	return ret;
}

/*
 * pmem2_unmap -- unmap the specified region
 */
int
pmem2_unmap(struct pmem2_map **map_ptr)
{
	LOG(3, "mapp %p", map_ptr);

	struct pmem2_map *map = *map_ptr;

	if (!UnmapViewOfFile(map->addr)) {
		ERR("!!UnmapViewOfFile");
		return pmem2_lasterror_to_err();
	}

	Free(map);
	*map_ptr = NULL;

	return PMEM2_E_OK;
}
