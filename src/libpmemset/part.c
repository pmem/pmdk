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
#include "os.h"
#include "part.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "ravl_interval.h"
#include "source.h"

struct pmemset_part {
	struct pmemset *set;
	size_t offset;
	size_t length;
	struct pmemset_file *file;
};

/*
 * pmemset_part_new -- creates a new part for the provided set
 */
int
pmemset_part_new(struct pmemset_part **part, struct pmemset *set,
		struct pmemset_source *src, size_t offset, size_t length)
{
	LOG(3, "part %p set %p src %p offset %zu length %zu",
			part, set, src, offset, length);
	PMEMSET_ERR_CLR();

	int ret;
	struct pmemset_part *partp;
	*part = NULL;

	ret = pmemset_source_validate(src);
	if (ret)
		return ret;

	partp = pmemset_malloc(sizeof(*partp), &ret);
	if (ret)
		return ret;

	ASSERTne(partp, NULL);

	partp->set = set;
	partp->offset = offset;
	partp->length = length;
	partp->file = pmemset_source_get_set_file(src);
	*part = partp;

	return ret;
}

/*
 * pmemset_part_delete -- deletes pmemset part
 */
int
pmemset_part_delete(struct pmemset_part **part)
{
	LOG(3, "part %p", part);
	PMEMSET_ERR_CLR();

	Free(*part);
	*part = NULL;

	return 0;
}

/*
 * memset_part_pread_mcsafe -- not supported
 */
int
pmemset_part_pread_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_pwrite_mcsafe -- not supported
 */
int
pmemset_part_pwrite_mcsafe(struct pmemset_part_descriptor *part,
		void *dst, size_t size, size_t offset)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_part_get_pmemset -- return set assigned to the part
 */
struct pmemset *
pmemset_part_get_pmemset(struct pmemset_part *part)
{
	return part->set;
}

/*
 * pmemset_part_map_init -- initialize the part map structure
 */
static int
pmemset_part_map_init(struct pmemset_part_map *pmap,
		struct pmem2_vm_reservation *pmem2_reserv)
{
	pmap->desc.addr = pmem2_vm_reservation_get_address(pmem2_reserv);
	pmap->desc.size = pmem2_vm_reservation_get_size(pmem2_reserv);
	pmap->pmem2_reserv = pmem2_reserv;
	pmap->refcount = 0;

	return 0;
}

/*
 * pmemset_part_map_new -- creates a new part mapping with a new part or extends
 *                         an existing part mapping by a new part
 */
int
pmemset_part_map_new(struct pmemset_part_map **pmap_ptr, size_t size)
{
	struct pmem2_vm_reservation *pmem2_reserv;
	int ret = pmem2_vm_reservation_new(&pmem2_reserv, NULL, size);
	if (ret) {
		if (ret == PMEM2_E_LENGTH_UNALIGNED) {
			ERR(
				"length %zu for the part mapping is not a multiple of %llu",
					size, Mmap_align);
			return PMEMSET_E_LENGTH_UNALIGNED;
		}
		return ret;
	}

	struct pmemset_part_map *pmap;
	pmap = pmemset_malloc(sizeof(*pmap), &ret);
	if (ret)
		goto err_p2rsv_delete;

	ret = pmemset_part_map_init(pmap, pmem2_reserv);
	if (ret)
		goto err_pmap_free;

	*pmap_ptr = pmap;

	return 0;

err_pmap_free:
	Free(pmap);
err_p2rsv_delete:
	pmem2_vm_reservation_delete(&pmem2_reserv);
	return ret;
}

/*
 * pmemset_part_map_delete -- deletes the part mapping and its contents
 */
int
pmemset_part_map_delete(struct pmemset_part_map **pmap_ptr)
{
	struct pmemset_part_map *pmap = *pmap_ptr;

	int ret = pmem2_vm_reservation_delete(&pmap->pmem2_reserv);
	if (ret)
		return ret;

	Free(pmap);
	*pmap_ptr = NULL;

	return 0;
}

/*
 * pmemset_part_map_extend_end -- extends a part map from the end by a given
 *                                size
 */
int
pmemset_part_map_extend_end(struct pmemset_part_map *pmap, size_t size)
{
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;

	int ret = pmem2_vm_reservation_extend(pmem2_reserv, size);
	if (ret)
		return ret;

	pmap->desc.size = pmem2_vm_reservation_get_size(pmem2_reserv);

	return 0;
}

/*
 * pmemset_part_map_shrink_end -- shrinks a part map from the end by a given
 *                                size
 */
int
pmemset_part_map_shrink_end(struct pmemset_part_map *pmap, size_t size)
{
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;
	size_t rsv_size = pmem2_vm_reservation_get_size(pmem2_reserv);

	size_t shrink_offset = rsv_size - size;
	int ret = pmem2_vm_reservation_shrink(pmem2_reserv, shrink_offset,
			size);
	if (ret)
		return ret;

	pmap->desc.size = pmem2_vm_reservation_get_size(pmem2_reserv);

	return 0;
}

/*
 * pmemset_part_map_shrink_start -- shrinks a part map from the start by a given
 *                                  size
 */
int
pmemset_part_map_shrink_start(struct pmemset_part_map *pmap, size_t size)
{
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;

	int ret = pmem2_vm_reservation_shrink(pmem2_reserv, 0, size);
	if (ret)
		return ret;

	pmap->desc.addr = pmem2_vm_reservation_get_address(pmem2_reserv);
	pmap->desc.size = pmem2_vm_reservation_get_size(pmem2_reserv);

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
		struct pmem2_map *map, void *arg)
{
	return pmem2_map_delete(&map);
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
 * pmemset_part_get_size -- returns part size
 */
size_t
pmemset_part_get_size(struct pmemset_part *part)
{
	return part->length;
}

/*
 * pmemset_part_get_offset -- returns part offset
 */
size_t
pmemset_part_get_offset(struct pmemset_part *part)
{
	return part->offset;
}

/*
 * pmemset_part_get_offset -- returns file associated with part
 */
struct pmemset_file *
pmemset_part_get_file(struct pmemset_part *part)
{
	return part->file;
}

/*
 * pmemset_part_file_try_ensure_size -- truncate part file if source
 * is from a temp file and if required
 */
int
pmemset_part_file_try_ensure_size(struct pmemset_part *part, size_t source_size)
{
	struct pmemset_file *f = part->file;
	bool truncate = pmemset_file_get_truncate(f);

	size_t size = part->offset + part->length;
	if (truncate && (size > source_size))
		return pmemset_file_truncate(f, size);

	return 0;
}
