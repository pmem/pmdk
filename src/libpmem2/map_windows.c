// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * map_windows.c -- pmem2_map (Windows)
 */

#include <stdbool.h>

#include "libpmem2.h"

#include "alloc.h"
#include "auto_flush.h"
#include "config.h"
#include "map.h"
#include "os_thread.h"
#include "out.h"
#include "persist.h"
#include "pmem2_utils.h"
#include "source.h"
#include "sys_util.h"
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
static HANDLE
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

struct pmem2_map *vm_reservation_map_find_closest_prior(
		struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len);
struct pmem2_map *vm_reservation_map_find_closest_later(
		struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len);

/*
 * reservation_mend -- unmaps given mapping and mends reservation area
 */
static int
reservation_mend(struct pmem2_vm_reservation *rsv, void *addr, size_t length)
{
	void *rsv_addr = pmem2_vm_reservation_get_address(rsv);
	size_t rsv_size = pmem2_vm_reservation_get_size(rsv);
	size_t rsv_offset = (size_t)addr - (size_t)rsv->addr;

	if (addr < rsv_addr ||
			(char *)addr + length > (char *)rsv_addr + rsv_size)
		return PMEM2_E_LENGTH_OUT_OF_RANGE;

	int ret = UnmapViewOfFile2(GetCurrentProcess(),
		addr,
		MEM_PRESERVE_PLACEHOLDER);
	if (!ret) {
		ERR("!!UnmapViewOfFile2");
		return pmem2_lasterror_to_err();
	}

	/*
	 * Before mapping to the reservation, it is neccessary to split
	 * the unoccupied region into separate placeholders, so that
	 * the mapping and the cut out placeholder will be of the same
	 * size.
	 */
	void *mend_addr = addr;
	size_t mend_size = length;
	struct pmem2_map *map = NULL;

	if (rsv_offset > 0) {
		map = vm_reservation_map_find_closest_prior(rsv, rsv_offset,
				length);
		if (map) {
			mend_addr = (char *)map->addr + map->reserved_length;
			mend_size += (char *)addr - (char *)mend_addr;
		} else {
			mend_addr = rsv->addr;
			mend_size += rsv_offset;
		}
	}

	if (rsv_offset + length < rsv_size) {
		map = vm_reservation_map_find_closest_later(rsv, rsv_offset,
				length);
		if (map)
			mend_size += (char *)map->addr - (char *)addr - length;
		else
			mend_size += rsv->size - rsv_offset - length;
	}

	if (addr != mend_addr) {
		ret = VirtualFree(mend_addr,
			mend_size,
			MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
		if (!ret) {
			ERR("!!VirtualFree");
			return pmem2_lasterror_to_err();

		}
	}

	return 0;
}

/*
 * reservation_split - splits the virtual memory reservation into
 *                     separate regions
 */
int
reservation_split(struct pmem2_vm_reservation *rsv, size_t rsv_offset,
		size_t length)
{
	void *rsv_addr = pmem2_vm_reservation_get_address(rsv);
	size_t rsv_size = pmem2_vm_reservation_get_size(rsv);

	if ((rsv_offset > 0 && !vm_reservation_map_find(rsv,
			rsv_offset - 1, 1)) ||
			(rsv_offset + length < rsv_size &&
			!vm_reservation_map_find(rsv,
			rsv_offset + length, 1))) {
		/* split the placeholder */
		int ret = VirtualFree((char *)rsv_addr + rsv_offset,
			length,
			MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
		if (!ret) {
			ERR("!!VirtualFree");
			ret = pmem2_lasterror_to_err();
			return ret;
		}
	}

	return 0;
}

/*
 * pmem2_map_new -- map memory according to provided config
 */
int
pmem2_map_new(struct pmem2_map **map_ptr, const struct pmem2_config *cfg,
		const struct pmem2_source *src)
{
	LOG(3, "cfg %p src %p map_ptr %p", cfg, src, map_ptr);
	PMEM2_ERR_CLR();

	int ret = 0;
	unsigned long err = 0;
	size_t file_size;
	*map_ptr = NULL;

	if ((int)cfg->requested_max_granularity == PMEM2_GRANULARITY_INVALID) {
		ERR(
			"please define the max granularity requested for the mapping");

		return PMEM2_E_GRANULARITY_NOT_SET;
	}

	ret = pmem2_source_size(src, &file_size);
	if (ret)
		return ret;

	size_t src_alignment;
	ret = pmem2_source_alignment(src, &src_alignment);
	if (ret)
		return ret;

	size_t length;
	ret = pmem2_config_validate_length(cfg, file_size, src_alignment);
	if (ret)
		return ret;

	size_t effective_offset;
	ret = pmem2_validate_offset(cfg, &effective_offset, src_alignment);
	if (ret)
		return ret;

	if (src->type == PMEM2_SOURCE_ANON)
		effective_offset = 0;

	/* without user-provided length, map to the end of the file */
	if (cfg->length)
		length = cfg->length;
	else
		length = file_size - effective_offset;

	HANDLE map_handle = INVALID_HANDLE_VALUE;
	if (src->type == PMEM2_SOURCE_HANDLE) {
		map_handle = src->value.handle;
	} else if (src->type == PMEM2_SOURCE_ANON) {
		/* no extra settings */
	} else {
		ASSERT(0);
	}

	DWORD proto = PAGE_READWRITE;
	DWORD access = FILE_MAP_ALL_ACCESS;

	/* Unsupported flag combinations */
	if ((cfg->protection_flag == PMEM2_PROT_NONE) ||
			(cfg->protection_flag == PMEM2_PROT_WRITE) ||
			(cfg->protection_flag == PMEM2_PROT_EXEC) ||
			(cfg->protection_flag == (PMEM2_PROT_WRITE |
					PMEM2_PROT_EXEC))) {
		ERR("Windows does not support "
				"this protection flag combination.");
		return PMEM2_E_NOSUPP;
	}

	/* Translate protection flags into Windows flags */
	if (cfg->protection_flag & PMEM2_PROT_WRITE) {
		if (cfg->protection_flag & PMEM2_PROT_EXEC) {
			proto = PAGE_EXECUTE_READWRITE;
			access = FILE_MAP_READ | FILE_MAP_WRITE |
			FILE_MAP_EXECUTE;
		} else {
			/*
			 * Due to the already done exclusion
			 * of incorrect combinations, PROT_WRITE
			 * implies PROT_READ
			 */
			proto = PAGE_READWRITE;
			access = FILE_MAP_READ | FILE_MAP_WRITE;
		}
	} else if (cfg->protection_flag & PMEM2_PROT_READ) {
		if (cfg->protection_flag & PMEM2_PROT_EXEC) {
			proto = PAGE_EXECUTE_READ;
			access = FILE_MAP_READ | FILE_MAP_EXECUTE;
		} else {
			proto = PAGE_READONLY;
			access = FILE_MAP_READ;
		}
	}

	if (cfg->sharing == PMEM2_PRIVATE) {
		if (cfg->protection_flag & PMEM2_PROT_EXEC) {
			proto = PAGE_EXECUTE_WRITECOPY;
			access = FILE_MAP_EXECUTE | FILE_MAP_COPY;
		} else {
			/*
			 * If FILE_MAP_COPY is set,
			 * protection is changed to read/write
			 */
			proto = PAGE_READONLY;
			access = FILE_MAP_COPY;
		}
	}

	/* create a file mapping handle */
	HANDLE mh = create_mapping(map_handle, effective_offset, length,
		proto, &err);

	if (!mh) {
		if (err == ERROR_ALREADY_EXISTS) {
			ERR("mapping already exists");
			return PMEM2_E_MAPPING_EXISTS;
		} else if (err == ERROR_ACCESS_DENIED) {
			return PMEM2_E_NO_ACCESS;
		}
		return pmem2_lasterror_to_err();
	}

	/* prepare pmem2_map structure */
	struct pmem2_map *map;
	map = (struct pmem2_map *)pmem2_malloc(sizeof(*map), &ret);
	if (!map)
		goto err_close_mapping_handle;

	void *base;
	if (cfg->reserv) {
		void *rsv = cfg->reserv;
		void *rsv_addr = pmem2_vm_reservation_get_address(rsv);
		size_t rsv_size = pmem2_vm_reservation_get_size(rsv);
		size_t rsv_offset = cfg->reserv_offset;

		/* check if reservation has enough space */
		if (rsv_offset + length > rsv_size) {
			ret = PMEM2_E_LENGTH_OUT_OF_RANGE;
			ERR(
				"reservation has not enought space, offset %zu, length %zu, rsv size %zu",
				rsv_offset, length, rsv_size);
			goto err_free_map_struct;
		}

		if (rsv_offset % Mmap_align) {
			ret = PMEM2_E_OFFSET_UNALIGNED;
			ERR(
				"virtual memory reservation offset %zu is not a multiple of %llu",
					rsv_offset, Mmap_align);
			goto err_free_map_struct;
		}

		map->addr = (char *)rsv_addr + rsv_offset;
		map->content_length = length;

		/* register wanted vm reservation region */
		ret = vm_reservation_map_register(cfg->reserv, map);
		if (ret)
			goto err_free_map_struct;

		/*
		 * Before mapping to the reservation, it is neccessary to split
		 * the unoccupied region into separate placeholders, so that
		 * the mapping and the cut out placeholder will be of the same
		 * size.
		 */
		util_rwlock_wrlock(&split_merge_lock);
		ret = reservation_split(rsv, rsv_offset, length);
		util_rwlock_unlock(&split_merge_lock);
		if (ret)
			goto err_vm_reserv_unregister;

		/* replace placeholder with a regular mapping */
		base = MapViewOfFile3(mh,
			NULL,
			(char *)rsv_addr + rsv_offset, /* addr in reservation */
			0,
			length,
			MEM_REPLACE_PLACEHOLDER,
			proto,
			NULL,
			0);

		if (base == NULL) {
			ERR("!!MapViewOfFile3");
			DWORD ret_windows = GetLastError();
			if (ret_windows == ERROR_INVALID_ADDRESS)
				ret = PMEM2_E_MAPPING_EXISTS;
			else
				ret = pmem2_lasterror_to_err();
			goto err_vm_reserv_unregister;
		}
	} else {
		/* obtain a pointer to the mapping view */
		base = MapViewOfFile(mh,
			access,
			HIDWORD(effective_offset),
			LODWORD(effective_offset),
			length);

		if (base == NULL) {
			ERR("!!MapViewOfFile");
			ret = pmem2_lasterror_to_err();
			goto err_free_map_struct;
		}
	}

	if (!CloseHandle(mh)) {
		ERR("!!CloseHandle");
		ret = pmem2_lasterror_to_err();
		goto err_unmap_base;
	}

	enum pmem2_granularity available_min_granularity =
		PMEM2_GRANULARITY_PAGE;
	if (src->type == PMEM2_SOURCE_HANDLE) {
		int direct_access = is_direct_access(src->value.handle);
		if (direct_access < 0) {
			ret = direct_access;
			goto err_unmap_base;
		}

		bool eADR = (pmem2_auto_flush() == 1);
		available_min_granularity =
			get_min_granularity(eADR, direct_access, cfg->sharing);
	} else if (src->type == PMEM2_SOURCE_ANON) {
		available_min_granularity = PMEM2_GRANULARITY_BYTE;
	} else {
		ASSERT(0);
	}

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

	map->addr = base;
	/*
	 * XXX probably in some cases the reserved length > the content length.
	 * Maybe it is worth to do the research.
	 */
	map->reserved_length = length;
	map->content_length = length;
	map->effective_granularity = available_min_granularity;
	map->reserv = cfg->reserv;
	map->source = *src;
	pmem2_set_flush_fns(map);
	pmem2_set_mem_fns(map);

	ret = pmem2_register_mapping(map);
	if (ret)
		goto err_unmap_base;

	/* return a pointer to the pmem2_map structure */
	*map_ptr = map;

	return ret;

err_unmap_base:
	/*
	 * if the reservation was given by pmem2_config, instead of unmapping,
	 * we will need to map with MAP_FIXED to mend the reservation
	 */
	if (cfg->reserv) {
		reservation_mend(cfg->reserv, base, length);
		vm_reservation_map_unregister(cfg->reserv, map);
	} else
		UnmapViewOfFile(base);
	free(map);

	return ret;

err_vm_reserv_unregister:
	vm_reservation_map_unregister(cfg->reserv, map);

err_free_map_struct:
	free(map);

err_close_mapping_handle:
	CloseHandle(mh);
	return ret;
}

/*
 * pmem2_map_delete -- unmap the specified region
 */
int
pmem2_map_delete(struct pmem2_map **map_ptr)
{
	LOG(3, "mapp %p", map_ptr);
	PMEM2_ERR_CLR();

	struct pmem2_map *map = *map_ptr;

	int ret = pmem2_unregister_mapping(map);
	if (ret)
		return ret;

	if (map->reserved_length != 0) {
		if (map->reserv) {
			util_rwlock_wrlock(&split_merge_lock);
			ret = reservation_mend(map->reserv, map->addr,
				map->reserved_length);
			util_rwlock_unlock(&split_merge_lock);
			if (ret)
				return ret;

			ret = vm_reservation_map_unregister(map->reserv, map);
			if (ret)
				return ret;
		} else {
			if (!UnmapViewOfFile(map->addr)) {
				ERR("!!UnmapViewOfFile");
				return pmem2_lasterror_to_err();
			}
		}
	}

	Free(map);
	*map_ptr = NULL;

	return 0;
}
