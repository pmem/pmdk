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
#include "auto_flush.h"
#include "out.h"
#include "config.h"
#include "map.h"
#include "pmem2_utils.h"
#include "alloc.h"
#include "util.h"

#define HIDWORD(x) ((DWORD)((x) >> 32))
#define LODWORD(x) ((DWORD)((x) & 0xFFFFFFFF))

/* requested CACHE_LINE, available PAGE */
#define REQ_CL_AVAIL_PG \
	"requested granularity not available because specified volume is not a direct access (DAX) volume"

/* requested BYTE, available PAGE */
#define REQ_BY_AVAIL_PG REQ_CL_AVAIL_PG

/* requested BYTE, available CACHE_LINE */
#define REQ_BY_AVAIL_CL \
	"requested granularity not available because the platform doesn't support eADR"

/* indicates the cases in which the error cannot occur */
#define GRAN_IMPOSSIBLE "impossible"
static const char *granularity_err_msg[3][3] = {
/*		requested granularity / available granularity		*/
/* -------------------------------------------------------------------- */
/*		BYTE		CACHE_LINE		PAGE		*/
/* -------------------------------------------------------------------- */
/* BYTE */ {GRAN_IMPOSSIBLE,	REQ_BY_AVAIL_CL,	REQ_BY_AVAIL_PG},
/* CL	*/ {GRAN_IMPOSSIBLE,	GRAN_IMPOSSIBLE,	REQ_CL_AVAIL_PG},
/* PAGE */ {GRAN_IMPOSSIBLE,	GRAN_IMPOSSIBLE,	GRAN_IMPOSSIBLE}};

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
 * is_direct_access -- check if the specified volume is a
 * direct access (DAX) volume
 */
static int
is_direct_access(HANDLE fh)
{
	DWORD filesystemFlags;

	if (!GetVolumeInformationByHandleW(fh, NULL, 0, NULL,
			NULL, &filesystemFlags, NULL, 0)) {
		ERR("!!GetVolumeInformationByHandleW");
		/* always return a negative value */
		return pmem2_lasterror_to_err();
	}

	if (filesystemFlags & FILE_DAX_VOLUME)
			return 1;

	return 0;
}

/*
 * pmem2_map -- map memory according to provided config
 */
int
pmem2_map(const struct pmem2_config *cfg, struct pmem2_map **map_ptr)
{
	LOG(3, "cfg %p map_ptr %p", cfg, map_ptr);

	int ret = 0;
	unsigned long err = 0;
	size_t file_size;
	*map_ptr = NULL;

	if (cfg->handle == INVALID_HANDLE_VALUE) {
		ERR("file handle was not set");
		return PMEM2_E_FILE_HANDLE_NOT_SET;
	}

	if ((int)cfg->requested_max_granularity == PMEM2_GRANULARITY_INVALID) {
		ERR(
			"please define the max granularity requested for the mapping");

		return PMEM2_E_GRANULARITY_NOT_SET;
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

	int direct_access = is_direct_access(cfg->handle);
	if (direct_access < 0) {
		ret = direct_access;
		goto err_unmap_base;
	}

	bool eADR = (pmem2_auto_flush() == 1);
	enum pmem2_granularity available_min_granularity =
		get_min_granularity(eADR, direct_access);

	if (available_min_granularity > cfg->requested_max_granularity) {
		const char *err = granularity_err_msg
			[cfg->requested_max_granularity]
			[available_min_granularity];
		if (strcmp(err, GRAN_IMPOSSIBLE) == 0)
			FATAL(
				"unhandled granularity error: available_min_granularity: %d" \
				"requested_max_granularity: %d",
				available_min_granularity,
				cfg->requested_max_granularity);
		ERR("%s", err);
		ret = PMEM2_E_GRANULARITY_NOT_SUPPORTED;
		goto err_unmap_base;
	}

	/* prepare pmem2_map structure */
	struct pmem2_map *map;
	map = (struct pmem2_map *)pmem2_malloc(sizeof(*map), &ret);
	if (!map)
		goto err_unmap_base;

	map->addr = base;
	/*
	 * XXX probably in some cases the reserved length > the content length.
	 * Maybe it is worth to do the research.
	 */
	map->reserved_length = length;
	map->content_length = length;
	map->effective_granularity = available_min_granularity;

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

	return 0;
}
