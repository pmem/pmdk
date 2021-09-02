// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * part.c -- implementation of common part API
 */

#include <fcntl.h>

#include "alloc.h"
#include "config.h"
#include "file.h"
#include "libpmemset.h"
#include "libpmem2.h"
#include "map_config.h"
#include "os.h"
#include "part.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "ravl_interval.h"
#include "sds.h"
#include "source.h"

/*
 * pmemset_part_map_init -- initialize the part map structure
 */
static int
pmemset_part_map_init(struct pmemset_part_map *pmap, struct pmemset *set,
		struct pmemset_source *src,
		struct pmem2_vm_reservation *pmem2_reserv, void *addr,
		size_t size)
{
	pmap->pmem2_reserv = pmem2_reserv;
	pmap->set = set;
	pmap->src = src;
	pmap->desc.addr = addr;
	pmap->desc.size = size;
	pmap->refcount = 0;

	return 0;
}

/*
 * pmemset_part_map_new -- creates a new part map structure
 */
int
pmemset_part_map_new(struct pmemset_part_map **pmap_ptr, struct pmemset *set,
		struct pmemset_source *src,
		struct pmem2_vm_reservation *pmem2_reserv, size_t offset,
		size_t size)
{
	int ret;
	struct pmemset_part_map *pmap;
	pmap = pmemset_malloc(sizeof(*pmap), &ret);
	if (ret)
		return ret;
	ASSERTne(pmap, NULL);

	void *addr = (char *)pmem2_vm_reservation_get_address(pmem2_reserv) +
			offset;
	ret = pmemset_part_map_init(pmap, set, src, pmem2_reserv, addr, size);
	if (ret)
		goto err_pmap_free;

	*pmap_ptr = pmap;

	return 0;

err_pmap_free:
	Free(pmap);
	return ret;
}

/*
 * pmemset_part_map_delete -- deletes the part mapping and its contents
 */
int
pmemset_part_map_delete(struct pmemset_part_map **pmap_ptr)
{
	Free(*pmap_ptr);
	*pmap_ptr = NULL;
	return 0;
}

/*
 * pmemset_part_map_iterate -- iterates over every pmem2 mappings stored in the
 * part mapping overlapping with the region defined by the offset and
 * size.
 */
int
pmemset_part_map_iterate(struct pmemset_part_map *pmap, size_t offset,
		size_t size, size_t *out_offset, size_t *out_size,
		pmemset_part_map_iter_cb cb, void *arg)
{
	struct pmem2_vm_reservation *pmem2_rsv = pmap->pmem2_reserv;

	size_t rsv_addr = (size_t)pmem2_vm_reservation_get_address(pmem2_rsv);

	/* offset needs to be relative to the vm reservation */
	offset += (size_t)pmap->desc.addr - (size_t)rsv_addr;
	size_t end_offset = offset + size;

	size_t initial_offset = SIZE_MAX;
	struct pmem2_map *map;
	while (end_offset > offset) {
		pmem2_vm_reservation_map_find(pmem2_rsv, offset, size, &map);
		if (!map)
			break;

		size_t map_addr = (size_t)pmem2_map_get_address(map);
		size_t map_offset = map_addr - rsv_addr;
		size_t map_size = pmem2_map_get_size(map);

		int ret = cb(pmap, map, arg);
		if (ret)
			return ret;

		/* mark the initial iteration offset */
		if (initial_offset == SIZE_MAX)
			initial_offset = map_offset;

		offset = map_offset + map_size;
		size = end_offset - offset;
	}

	if (out_offset)
		*out_offset = initial_offset;
	if (out_size)
		*out_size = offset - initial_offset;

	return 0;
}

/*
 * pmemset_pmem2_map_delete_cb -- wrapper for deleting pmem2 mapping on each
 *                                iteration
 */
static int
pmemset_pmem2_map_delete_cb(struct pmemset_part_map *pmap,
		struct pmem2_map *p2map, void *arg)
{
	struct pmemset *set = pmap->set;
	struct pmemset_sds_record *sds_record = pmemset_sds_find_record(p2map,
			set);

	int ret = pmem2_map_delete(&p2map);
	if (ret)
		return ret;

	if (sds_record) {
		struct pmemset_config *cfg = pmemset_get_config(set);
		pmemset_sds_unregister_record_fire_event(sds_record, set, cfg);
	}

	return 0;
}

/*
 * pmemset_part_map_remove_range -- removes the memory range belonging to the
 *                                  part mapping
 */
int
pmemset_part_map_remove_range(struct pmemset_part_map *pmap, size_t offset,
		size_t size, size_t *out_offset, size_t *out_size)
{
	int ret = pmemset_part_map_iterate(pmap, offset, size, out_offset,
			out_size, pmemset_pmem2_map_delete_cb, NULL);
	if (ret) {
		if (ret == PMEM2_E_MAPPING_NOT_FOUND)
			return PMEMSET_E_CANNOT_FIND_PART_MAP;
		return ret;
	}

	return 0;
}

/*
 * pmemset_part_file_try_ensure_size -- grow part file if source
 * is from a temp file and by default, if not specified otherwise
 */
int
pmemset_part_file_try_ensure_size(struct pmemset_file *f, size_t len,
		size_t off, size_t source_size)
{
	int ret = 0;
	bool grow = pmemset_file_get_grow(f);

	size_t s = off + len;
	if (!grow && (s > source_size)) {
		ERR(
			"file cannot be extended but its size equals %zu, "
		    "while it should equal at least %zu to map the part",
				source_size, s);
		return PMEMSET_E_SOURCE_FILE_IS_TOO_SMALL;
	}
	if (grow && (s > source_size))
		ret = pmemset_file_grow(f, s);

	if (ret) {
		ERR("cannot extend source from the part file %p", f);
		return PMEMSET_E_CANNOT_GROW_SOURCE_FILE;
	}

	return 0;
}

/*
 * pmemset_part_map_find -- find the earliest pmem2 mapping in the provied range
 */
int
pmemset_part_map_find(struct pmemset_part_map *pmap, size_t offset, size_t size,
	struct pmem2_map **p2map)
{
	int ret;
	*p2map = NULL;

	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;
	size_t rsv_addr = (size_t)pmem2_vm_reservation_get_address(
			pmem2_reserv);
	size_t pmap_addr = (size_t)pmap->desc.addr;

	ASSERT(pmap_addr >= rsv_addr);
	/* part map offset in regards to the vm reservation */
	size_t pmap_offset = pmap_addr - rsv_addr;

	ret = pmem2_vm_reservation_map_find(pmem2_reserv, pmap_offset + offset,
			size, p2map);
	if (ret) {
		if (ret == PMEM2_E_MAPPING_NOT_FOUND) {
			ERR(
				"no part found at the range (offset %zu, size %zu) "
				"in the part mapping %p", offset, size, pmap);
			return PMEMSET_E_PART_NOT_FOUND;
		}
		return ret;
	}

	return 0;
}
