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
int
pmemset_part_map_init(struct pmemset_part_map **map,
		struct pmem2_vm_reservation *pmem2_reserv)
{
	int ret;
	struct pmemset_part_map *pmap;
	pmap = pmemset_malloc(sizeof(*pmap), &ret);
	if (ret)
		return ret;

	pmap->desc.addr = pmem2_vm_reservation_get_address(
			pmem2_reserv);
	pmap->desc.size = pmem2_vm_reservation_get_size(pmem2_reserv);
	pmap->pmem2_reserv = pmem2_reserv;
	pmap->refcount = 0;
	*map = pmap;

	return 0;
}

/*
 * pmemset_part_map_new -- map a part and create a structure
 *                         that describes the mapping
 */
int
pmemset_part_map_new(struct pmemset_part_map **part_map,
		struct pmemset_part *part, enum pmem2_granularity gran,
		enum pmem2_granularity *mapping_gran,
		struct pmemset_part_map *prev_pmap,
		enum pmemset_coalescing part_coalescing)
{
	*part_map = NULL;

	struct pmem2_config *pmem2_cfg;
	int ret = pmem2_config_new(&pmem2_cfg);
	if (ret) {
		ERR("cannot create pmem2_config %d", ret);
		return PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE;
	}

	ret = pmem2_config_set_length(pmem2_cfg, part->length);
	ASSERTeq(ret, 0);

	ret = pmem2_config_set_offset(pmem2_cfg, part->offset);
	if (ret) {
		ERR("invalid value of pmem2_config offset %zu", part->offset);
		ret = PMEMSET_E_INVALID_OFFSET_VALUE;
		goto err_cfg_delete;
	}

	ret = pmem2_config_set_required_store_granularity(pmem2_cfg, gran);
	if (ret) {
		ERR("granularity value is not supported %d", ret);
		ret = PMEMSET_E_GRANULARITY_NOT_SUPPORTED;
		goto err_cfg_delete;
	}

	struct pmem2_source *pmem2_src;
	pmem2_src = pmemset_file_get_pmem2_source(part->file);

	size_t part_size = part->length;
	if (part_size == 0)
		pmem2_source_size(pmem2_src, &part_size);

	struct pmem2_vm_reservation *pmem2_reserv;
	size_t offset;
	bool coalesced = true;

	switch (part_coalescing) {
		case PMEMSET_COALESCING_OPPORTUNISTIC:
		case PMEMSET_COALESCING_FULL:
			if (prev_pmap) {
				pmem2_reserv = prev_pmap->pmem2_reserv;
				offset = pmem2_vm_reservation_get_size(
						pmem2_reserv);
				ret = pmem2_vm_reservation_extend(pmem2_reserv,
						part_size);

				if (part_coalescing ==
						PMEMSET_COALESCING_FULL || !ret)
					break;
			}
		case PMEMSET_COALESCING_NONE:
			offset = 0;
			ret = pmem2_vm_reservation_new(&pmem2_reserv, NULL,
					part_size);
			coalesced = false;
			break;
		default:
			ERR("invalid coalescing value %d", part_coalescing);
			return PMEMSET_E_INVALID_COALESCING_VALUE;
	}

	if (ret) {
		if (ret == PMEM2_E_MAPPING_EXISTS) {
			ERR(
				"new part %p couldn't be coalesced with the previous part, "
				"the memory range after the previous mapped part is occupied",
					part);
			ret = PMEMSET_E_CANNOT_COALESCE_PARTS;
		} else if (ret == PMEM2_E_LENGTH_UNALIGNED) {
			ERR("part length %zu is not a multiple of %llu",
					part->length, Mmap_align);
			ret = PMEMSET_E_LENGTH_UNALIGNED;
		}
		goto err_cfg_delete;
	}

	pmem2_config_set_vm_reservation(pmem2_cfg, pmem2_reserv, offset);

	struct pmem2_map *pmem2_map;
	ret = pmem2_map_new(&pmem2_map, pmem2_cfg, pmem2_src);
	if (ret) {
		ERR("cannot create pmem2 mapping %d", ret);
		ret = PMEMSET_E_INVALID_PMEM2_MAP;
		if (coalesced) {
			size_t offset = pmem2_vm_reservation_get_size(
					pmem2_reserv) - part_size;
			pmem2_vm_reservation_shrink(pmem2_reserv, offset,
					part_size);
		} else {
			pmem2_vm_reservation_delete(&pmem2_reserv);
		}
		goto err_cfg_delete;
	}

	if (!coalesced) {
		struct pmemset_part_map *pmap;
		ret = pmemset_part_map_init(&pmap, pmem2_reserv);
		if (ret)
			return ret;

		*part_map = pmap;
	} else {
		prev_pmap->desc.size = pmem2_vm_reservation_get_size(
				pmem2_reserv);
	}

	*mapping_gran = pmem2_map_get_store_granularity(pmem2_map);

err_cfg_delete:
	pmem2_config_delete(&pmem2_cfg);
	return ret;
}

/*
 * pmemset_part_map_delete -- unmap the part map and delete the
 *                            structure that describes the mapping
 */
void
pmemset_part_map_delete(struct pmemset_part_map **part_map)
{
	struct pmemset_part_map *pmap = *part_map;
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;

	void *rsv_addr = pmem2_vm_reservation_get_address(pmem2_reserv);
	size_t rsv_size = pmem2_vm_reservation_get_size(pmem2_reserv);

	struct pmem2_map *map;
	/* find pmem2 mapping located at the end of the reservation */
	pmem2_vm_reservation_map_find(pmem2_reserv, rsv_size - 1, 1, &map);
	ASSERTne(map, NULL);

	void *pmem2_map_addr = pmem2_map_get_address(map);
	size_t pmem2_map_size = pmem2_map_get_size(map);

	pmem2_map_delete(&map);

	if (rsv_addr == pmem2_map_addr && rsv_size == pmem2_map_size) {
		/* pmem2 mapping covers whole vm reservation */
		pmem2_vm_reservation_delete(&pmem2_reserv);
		Free(pmap);
	} else {
		/* pmem2 mapping covers part of the vm reservation */
		size_t offset = (size_t)pmem2_map_addr - (size_t)rsv_addr;
		pmem2_vm_reservation_shrink(pmem2_reserv, offset,
				pmem2_map_size);
	}

	*part_map = NULL;
}

/*
 * pmemset_part_map_delete_with_contents -- removes the part map structure and
 *                                          unmaps contained pmem2 mappings
 */
int
pmemset_part_map_delete_with_contents(struct pmemset_part_map **part_ptr)
{
	int ret;
	struct pmemset_part_map *pmap = *part_ptr;
	struct pmem2_vm_reservation *rsv = pmap->pmem2_reserv;
	size_t rsv_size = pmem2_vm_reservation_get_size(rsv);

	struct pmem2_map *map;
	pmem2_vm_reservation_map_find(rsv, 0, rsv_size, &map);
	while (map) {
		ret = pmem2_map_delete(&map);
		if (ret)
			return ret;

		pmem2_vm_reservation_map_find(rsv, 0, rsv_size, &map);
	}

	ret = pmem2_vm_reservation_delete(&rsv);
	if (ret)
		return ret;

	Free(pmap);
	*part_ptr = NULL;

	return 0;
}
