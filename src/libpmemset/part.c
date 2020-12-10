// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

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
	struct pmemset_config *set_config = pmemset_get_pmemset_config(set);
	*part = NULL;

	ret = pmemset_source_validate(src);
	if (ret)
		return ret;

	partp = pmemset_malloc(sizeof(*partp), &ret);
	if (ret)
		return ret;

	ASSERTne(partp, NULL);

	ret = pmemset_source_create_pmemset_file(src, &partp->file, set_config);

	if (ret)
		goto err_free_part;

	partp->set = set;
	partp->offset = offset;
	partp->length = length;

	*part = partp;

	return 0;

err_free_part:
	Free(partp);
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

	pmemset_file_delete(&(*part)->file);
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
 * pmemset_part_map_by_address -- not supported
 */
int
pmemset_part_map_by_address(struct pmemset *set, struct pmemset_part **part,
		void *addr)
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
 * pmemset_part_map_new -- map a part and create a structure
 *                         that describes the mapping
 */
int
pmemset_part_map_new(struct pmemset_part_map **part_map,
		struct pmemset_part *part, enum pmem2_granularity gran,
		enum pmem2_granularity *mapping_gran,
		struct pmemset_part_descriptor previous_part)
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

	struct pmemset_part_map *pmap;
	pmap = pmemset_malloc(sizeof(*pmap), &ret);
	if (ret)
		goto err_cfg_delete;

	struct pmem2_source *pmem2_src;
	pmem2_src = pmemset_file_get_pmem2_source(part->file);

	/* mapping with a hint address next to previous mapped part */
	void *contiguous_addr = (char *)previous_part.addr + previous_part.size;
	size_t part_size = part->length;
	if (part_size == 0)
		pmem2_source_size(pmem2_src, &part_size);

	struct pmem2_vm_reservation *pmem2_reserv;
	pmem2_vm_reservation_new(&pmem2_reserv, contiguous_addr, part_size);

	pmem2_config_set_vm_reservation(pmem2_cfg, pmem2_reserv, 0);

	struct pmem2_map *pmem2_map;
	ret = pmem2_map_new(&pmem2_map, pmem2_cfg, pmem2_src);
	if (ret) {
		ERR("cannot create pmem2 mapping %d", ret);
		ret = PMEMSET_E_INVALID_PMEM2_MAP;
		Free(pmap);
		if (pmem2_reserv)
			pmem2_vm_reservation_delete(&pmem2_reserv);
		goto err_cfg_delete;
	}

	*mapping_gran = pmem2_map_get_store_granularity(pmem2_map);
	pmap->desc.addr = pmem2_map_get_address(pmem2_map);
	pmap->desc.size = pmem2_map_get_size(pmem2_map);
	pmap->pmem2_map = pmem2_map;
	pmap->pmem2_reserv = pmem2_reserv;
	pmap->refcount = 0;
	*part_map = pmap;

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
	pmem2_map_delete(&pmap->pmem2_map);
	if (pmap->pmem2_reserv)
		pmem2_vm_reservation_delete(&pmap->pmem2_reserv);
	Free(pmap);
	*part_map = NULL;
}

/*
 * pmemset_part_map_drop -- drops the reference to the part map through provided
 *                          ponter. Doesn't delete part map.
 */
void
pmemset_part_map_drop(struct pmemset_part_map **pmap)
{
	LOG(3, "pmap %p", pmap);

	(*pmap)->refcount -= 1;
	*pmap = NULL;
}

/*
 * pmemset_part_mapping_inc_count -- increases the reference count of the
 *                                   provided part map by 1
 */
void
pmemset_part_mapping_inc_count(struct pmemset_part_map *pmap)
{
	pmap->refcount += 1;
}

/*
 * pmemset_part_mapping_dec_count -- decreases the reference count of the
 *                                   provided part map by 1
 */
void
pmemset_part_mapping_dec_count(struct pmemset_part_map *pmap)
{
	pmap->refcount -= 1;
}
