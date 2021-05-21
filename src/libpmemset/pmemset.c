// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * pmemset.c -- implementation of common pmemset API
 */

#include <stdbool.h>

#include "alloc.h"
#include "config.h"
#include "file.h"
#include "libpmem2.h"
#include "libpmemset.h"
#include "part.h"
#include "pmemset.h"
#include "pmemset_utils.h"
#include "ravl_interval.h"
#include "ravl.h"
#include "sys_util.h"

/*
 * pmemset
 */
struct pmemset {
	struct pmemset_config *set_config;
	bool effective_granularity_valid;
	enum pmem2_granularity effective_granularity;
	enum pmemset_coalescing part_coalescing;
	pmem2_persist_fn persist_fn;
	pmem2_flush_fn flush_fn;
	pmem2_drain_fn drain_fn;
	pmem2_memmove_fn memmove_fn;
	pmem2_memset_fn memset_fn;
	pmem2_memcpy_fn memcpy_fn;

	struct pmemset_shared_state {
		os_rwlock_t lock;
		struct ravl_interval *part_map_tree;
		struct pmemset_part_map *previous_pmap;
	} shared_state;
};

static char *granularity_name[3] = {
	"PMEM2_GRANULARITY_BYTE",
	"PMEM2_GRANULARITY_CACHE_LINE",
	"PMEM2_GRANULARITY_PAGE"
};

/*
 * pmemset_mapping_min
 */
static size_t
pmemset_mapping_min(void *addr)
{
	if (addr == NULL)
		return 0;

	struct pmemset_part_map *pmap = (struct pmemset_part_map *)addr;
	return (size_t)pmap->desc.addr;
}

/*
 * pmemset_mapping_max
 */
static size_t
pmemset_mapping_max(void *addr)
{
	if (addr == NULL)
		return SIZE_MAX;

	struct pmemset_part_map *pmap = (struct pmemset_part_map *)addr;
	void *pmap_addr = pmap->desc.addr;
	size_t pmap_size = pmap->desc.size;
	return (size_t)pmap_addr + pmap_size;
}

/*
 * pmemset_new_init -- initialize set structure.
 */
static int
pmemset_new_init(struct pmemset *set, struct pmemset_config *config)
{
	ASSERTne(config, NULL);

	int ret;

	/* duplicate config */
	set->set_config = NULL;
	ret = pmemset_config_duplicate(&set->set_config, config);
	if (ret)
		return ret;

	/* initialize RAVL */
	set->shared_state.part_map_tree = ravl_interval_new(pmemset_mapping_min,
						pmemset_mapping_max);

	if (set->shared_state.part_map_tree == NULL) {
		ERR("ravl tree initialization failed");
		return PMEMSET_E_ERRNO;
	}

	set->effective_granularity_valid = false;
	set->shared_state.previous_pmap = NULL;
	set->part_coalescing = PMEMSET_COALESCING_NONE;

	set->persist_fn = NULL;
	set->flush_fn = NULL;
	set->drain_fn = NULL;

	set->memmove_fn = NULL;
	set->memset_fn = NULL;
	set->memcpy_fn = NULL;

	util_rwlock_init(&set->shared_state.lock);

	return 0;
}

/*
 * pmemset_new -- allocates and initialize pmemset structure.
 */
int
pmemset_new(struct pmemset **set, struct pmemset_config *cfg)
{
	PMEMSET_ERR_CLR();

	if (pmemset_get_config_granularity_valid(cfg) == false) {
		ERR(
			"please define the max granularity requested for the mapping");

		return PMEMSET_E_GRANULARITY_NOT_SET;
	}

	int ret = 0;

	/* allocate set structure */
	*set = pmemset_malloc(sizeof(**set), &ret);

	if (ret)
		return ret;

	ASSERTne(set, NULL);

	/* initialize set */
	ret = pmemset_new_init(*set, cfg);
	if (ret) {
		Free(*set);
		*set = NULL;
	}

	return ret;
}

/*
 * pmemset_adjust_reservation_to_contents -- adjust vm reservation boundaries
 * to the earliest and latest pmem2 mapping stored, delete whole vm reservation
 * if it's empty
 */
static int
pmemset_adjust_reservation_to_contents(
		struct pmem2_vm_reservation **pmem2_reserv)
{
	int ret;
	struct pmem2_vm_reservation *p2rsv = *pmem2_reserv;

	size_t rsv_addr = (size_t)pmem2_vm_reservation_get_address(p2rsv);
	size_t rsv_size = pmem2_vm_reservation_get_size(p2rsv);

	struct pmem2_map *p2map;
	/* find first pmem2 mapping in the vm reservation */
	pmem2_vm_reservation_map_find_first(p2rsv, &p2map);

	if (!p2map) {
		/* vm reservation is empty so it needs to be deleted */
		ret = pmem2_vm_reservation_delete(pmem2_reserv);
		ASSERTeq(ret, 0);
	} else {
		/* vm reservation is not empty so it needs to be resized */
		size_t p2map_offset = (size_t)pmem2_map_get_address(p2map) -
				rsv_addr;
		size_t p2map_size = pmem2_map_get_size(p2map);

		if (p2map_offset > 0) {
			ret = pmem2_vm_reservation_shrink(p2rsv, 0,
					p2map_offset);
			ASSERTeq(ret, 0);
		}

		/* find last pmem2 mapping in the vm reservation */
		ret = pmem2_vm_reservation_map_find_last(p2rsv, &p2map);

		p2map_offset = (size_t)pmem2_map_get_address(p2map) - rsv_addr;
		p2map_size = pmem2_map_get_size(p2map);

		if (p2map_offset + p2map_size < rsv_size) {
			size_t shrink_offset = p2map_offset + p2map_size;
			size_t shrink_size = rsv_size - shrink_offset;
			ret = pmem2_vm_reservation_shrink(p2rsv, shrink_offset,
					shrink_size);
			ASSERTeq(ret, 0);
		}
	}

	return 0;
}

/*
 * pmemset_delete_pmap_ravl_arg -- arguments for pmap deletion callback
 */
struct pmemset_delete_pmap_ravl_arg {
	int ret;
	bool adjust_reservation;
};

/*
 * pmemset_delete_all_pmaps_ravl_cb -- unmaps and deletes part mappings stored
 *                                     in the ravl interval tree
 */
static void
pmemset_delete_pmap_ravl_cb(void *data, void *arg)
{
	struct pmemset_part_map **pmap_ptr = (struct pmemset_part_map **)data;
	struct pmemset_part_map *pmap = *pmap_ptr;

	struct pmemset_delete_pmap_ravl_arg *cb_args =
			(struct pmemset_delete_pmap_ravl_arg *)arg;

	int *ret = &cb_args->ret;

	size_t pmap_size = pmemset_descriptor_part_map(pmap).size;
	*ret = pmemset_part_map_remove_range(pmap, 0, pmap_size, NULL, NULL);
	if (*ret)
		return;

	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;
	*ret = pmemset_part_map_delete(pmap_ptr);
	if (*ret)
		return;

	/* reservation provided by the user should not be modified */
	if (cb_args->adjust_reservation)
		*ret = pmemset_adjust_reservation_to_contents(&pmem2_reserv);
}

/*
 * pmemset_delete -- de-allocate set structure
 */
int
pmemset_delete(struct pmemset **set)
{
	LOG(3, "pmemset %p", set);
	PMEMSET_ERR_CLR();

	if (*set == NULL)
		return 0;

	struct pmemset_config *cfg = pmemset_get_pmemset_config(*set);
	struct pmem2_vm_reservation *rsv = pmemset_config_get_reservation(cfg);

	/* reservation that was set in pmemset should not be adjusted */
	struct pmemset_delete_pmap_ravl_arg arg;
	arg.ret = 0;
	arg.adjust_reservation = (rsv == NULL);

	/* delete RAVL tree with part_map nodes */
	ravl_interval_delete_cb((*set)->shared_state.part_map_tree,
			pmemset_delete_pmap_ravl_cb, &arg);
	if (arg.ret)
		return arg.ret;

	/* delete cfg */
	pmemset_config_delete(&(*set)->set_config);

	util_rwlock_destroy(&(*set)->shared_state.lock);

	Free(*set);

	*set = NULL;

	return 0;
}

/*
 * pmemset_insert_part_map -- insert part mapping into the ravl interval tree
 */
static int
pmemset_insert_part_map(struct pmemset *set, struct pmemset_part_map *map)
{
	int ret = ravl_interval_insert(set->shared_state.part_map_tree, map);
	if (ret == 0) {
		return 0;
	} else if (ret == -EEXIST) {
		ERR("part already exists");
		return PMEMSET_E_PART_EXISTS;
	} else {
		return PMEMSET_E_ERRNO;
	}
}

/*
 * pmemset_unregister_part_map -- unregister part mapping from the ravl
 *                                interval tree
 */
static int
pmemset_unregister_part_map(struct pmemset *set, struct pmemset_part_map *map)
{
	int ret = 0;

	struct ravl_interval *tree = set->shared_state.part_map_tree;
	struct ravl_interval_node *node = ravl_interval_find_equal(tree, map);

	if (!(node && !ravl_interval_remove(tree, node))) {
		ERR("cannot find part mapping %p in the set %p", map, set);
		ret = PMEMSET_E_PART_NOT_FOUND;
	}

	return ret;
}

/*
 * pmemset_set_store_granularity -- set effective_graunlarity
 * in the pmemset structure
 */
static void
pmemset_set_store_granularity(struct pmemset *set, enum pmem2_granularity g)
{
	LOG(3, "set %p g %d", set, g);
	set->effective_granularity = g;
}

/*
 * pmemset_get_store_granularity -- get effective_graunlarity
 * from pmemset
 */
int
pmemset_get_store_granularity(struct pmemset *set, enum pmem2_granularity *g)
{
	LOG(3, "%p", set);

	if (set->effective_granularity_valid == false) {
		ERR(
			"effective granularity value for pmemset is not set, no part is mapped");
		return PMEMSET_E_NO_PART_MAPPED;
	}

	*g = set->effective_granularity;

	return 0;
}

/*
 * pmemset_set_persisting_fn -- sets persist, flush and
 * drain functions for pmemset
 */
static void
pmemset_set_persisting_fn(struct pmemset *set, struct pmemset_part_map *pmap)
{
	if (!pmap)
		return;

	struct pmem2_map *p2m;
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;
	size_t pmem2_reserv_size = pmem2_vm_reservation_get_size(pmem2_reserv);
	pmem2_vm_reservation_map_find(pmem2_reserv, 0, pmem2_reserv_size, &p2m);
	ASSERTne(p2m, NULL);

	/* should be set only once per pmemset */
	if (!set->persist_fn)
		set->persist_fn = pmem2_get_persist_fn(p2m);
	if (!set->flush_fn)
		set->flush_fn = pmem2_get_flush_fn(p2m);
	if (!set->drain_fn)
		set->drain_fn = pmem2_get_drain_fn(p2m);
}

/*
 * pmemset_set_mem_fn -- sets pmem2  memset, memmove, memcpy
 * functions for pmemset
 */
static void
pmemset_set_mem_fn(struct pmemset *set, struct pmemset_part_map *pmap)
{
	if (!pmap)
		return;

	struct pmem2_map *p2m;
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;
	size_t pmem2_reserv_size = pmem2_vm_reservation_get_size(pmem2_reserv);
	pmem2_vm_reservation_map_find(pmem2_reserv, 0, pmem2_reserv_size, &p2m);
	ASSERTne(p2m, NULL);

	/* should be set only once per pmemset */
	if (!set->memmove_fn)
		set->memmove_fn = pmem2_get_memmove_fn(p2m);
	if (!set->memset_fn)
		set->memset_fn = pmem2_get_memset_fn(p2m);
	if (!set->memcpy_fn)
		set->memcpy_fn = pmem2_get_memcpy_fn(p2m);
}

/*
 * pmemset_pmem2_config_init -- initialize pmem2 config structure
 */
static int
pmemset_pmem2_config_init(struct pmem2_config *pmem2_cfg,
		size_t size, size_t offset, enum pmem2_granularity gran)
{
	int ret = pmem2_config_set_length(pmem2_cfg, size);
	ASSERTeq(ret, 0);

	ret = pmem2_config_set_offset(pmem2_cfg, offset);
	if (ret) {
		ERR("invalid value of pmem2_config offset %zu", offset);
		return PMEMSET_E_INVALID_OFFSET_VALUE;
	}

	ret = pmem2_config_set_required_store_granularity(pmem2_cfg, gran);
	if (ret) {
		ERR("granularity value is not supported %d", ret);
		return PMEMSET_E_GRANULARITY_NOT_SUPPORTED;
	}

	return 0;
}

/*
 * pmemset_create_vm_reservation -- create vm reservation with an arbitrarily
 *                                  chosen address and a given size
 */
static int
pmemset_create_reservation(struct pmem2_vm_reservation **pmem2_reserv,
		size_t size)
{
	struct pmem2_vm_reservation *p2rsv;
	int ret = pmem2_vm_reservation_new(&p2rsv, NULL, size);
	if (ret == PMEM2_E_LENGTH_UNALIGNED)
		return PMEMSET_E_LENGTH_UNALIGNED;
	else if (ret)
		return ret;

	*pmem2_reserv = p2rsv;

	return 0;
}

enum reservation_prep_type {
	RESERV_PREP_TYPE_CHECK_IF_OCCUPIED,
	RESERV_PREP_TYPE_EXTEND_IF_NEEDED
};

/*
 * pmemset_prepare_reservation_range -- prepare the reservation memory range for
 *                                      the mapping
 */
static int
pmemset_prepare_reservation_range(struct pmem2_vm_reservation *p2rsv,
		size_t offset, size_t size, enum reservation_prep_type type)
{
	int ret = 0;

	size_t p2rsv_size = pmem2_vm_reservation_get_size(p2rsv);
	ASSERT(offset <= p2rsv_size);

	/* check if desired memory range after pmap is occupied */
	struct pmem2_map *p2map;
	pmem2_vm_reservation_map_find(p2rsv, offset, size, &p2map);
	if (p2map)
		return PMEMSET_E_PART_EXISTS;

	if (type == RESERV_PREP_TYPE_EXTEND_IF_NEEDED &&
			offset + size > p2rsv_size) {
		/* extend the reservation to fit desired range */
		size_t extend_size = size - (p2rsv_size - offset);
		ret = pmem2_vm_reservation_extend(p2rsv, extend_size);
		if (ret == PMEM2_E_MAPPING_EXISTS)
			return PMEMSET_E_PART_EXISTS;
	}

	return ret;
}

/*
 * pmemset_find_reservation_empty_range -- find empty range in given reservation
 */
static int
pmemset_find_reservation_empty_range(struct pmem2_vm_reservation *p2rsv,
		size_t size, size_t *out_offset)
{
	*out_offset = SIZE_MAX;

	size_t p2rsv_addr = (size_t)pmem2_vm_reservation_get_address(p2rsv);
	size_t p2rsv_size = pmem2_vm_reservation_get_size(p2rsv);

	struct pmem2_map *any_p2map;
	size_t search_offset = 0;
	while (search_offset + size <= p2rsv_size) {
		pmem2_vm_reservation_map_find(p2rsv, search_offset, size,
				&any_p2map);
		if (!any_p2map) {
			*out_offset = search_offset;
			return 0;
		}

		size_t p2map_addr = (size_t)pmem2_map_get_address(any_p2map);
		size_t p2map_size = (size_t)pmem2_map_get_size(any_p2map);

		search_offset = p2map_addr + p2map_size - p2rsv_addr;
	}

	ERR("reservation %p with reservation size %zu could not fit a part "
		"mapping with size %zu at any offset, possible reservation "
		"ranges could already be occupied", p2rsv, p2rsv_size, size);
	return PMEMSET_E_CANNOT_FIT_PART_MAP;
}

/*
 * pmemset_part_map -- map a part to the set
 */
int
pmemset_part_map(struct pmemset_part **part_ptr, struct pmemset_extras *extra,
		struct pmemset_part_descriptor *desc)
{
	LOG(3, "part %p extra %p desc %p", part_ptr, extra, desc);
	PMEMSET_ERR_CLR();

	struct pmemset_part *part = *part_ptr;
	struct pmemset *set = pmemset_part_get_pmemset(part);
	struct pmemset_config *set_config = pmemset_get_pmemset_config(set);
	enum pmem2_granularity mapping_gran;
	enum pmem2_granularity config_gran =
			pmemset_get_config_granularity(set_config);

	size_t part_offset = pmemset_part_get_offset(part);
	struct pmemset_file *part_file = pmemset_part_get_file(part);
	struct pmem2_source *pmem2_src =
			pmemset_file_get_pmem2_source(part_file);

	size_t part_size = pmemset_part_get_size(part);
	size_t source_size;
	int ret = pmem2_source_size(pmem2_src, &source_size);
	if (ret)
		return ret;

	if (part_size == 0)
		part_size = source_size;

	ret = pmemset_part_file_try_ensure_size(part, source_size);
	if (ret) {
		ERR("cannot truncate source file from the part %p", part);
		ret = PMEMSET_E_CANNOT_TRUNCATE_SOURCE_FILE;
		return ret;
	}

	/* setup temporary pmem2 config */
	struct pmem2_config *pmem2_cfg;
	ret = pmem2_config_new(&pmem2_cfg);
	if (ret) {
		ERR("cannot create pmem2_config %d", ret);
		return PMEMSET_E_CANNOT_ALLOCATE_INTERNAL_STRUCTURE;
	}

	ret = pmemset_pmem2_config_init(pmem2_cfg, part_size, part_offset,
			config_gran);
	if (ret)
		goto err_pmem2_cfg_delete;

	/* lock the pmemset */
	util_rwlock_wrlock(&set->shared_state.lock);

	bool coalesced = true;
	struct pmemset_part_map *pmap = NULL;
	struct pmem2_vm_reservation *pmem2_reserv;
	struct pmem2_vm_reservation *config_rsv =
			pmemset_config_get_reservation(set_config);
	size_t map_reserv_offset;
	enum pmemset_coalescing coalescing = set->part_coalescing;
	switch (coalescing) {
		case PMEMSET_COALESCING_OPPORTUNISTIC:
		case PMEMSET_COALESCING_FULL:
			/* if no prev pmap then skip this, but don't fail */
			if (set->shared_state.previous_pmap) {
				pmap = set->shared_state.previous_pmap;
				pmem2_reserv = pmap->pmem2_reserv;
				void *p2rsv_addr;
				p2rsv_addr = pmem2_vm_reservation_get_address(
						pmem2_reserv);
				map_reserv_offset = (size_t)pmap->desc.addr +
						pmap->desc.size -
						(size_t)p2rsv_addr;

				enum reservation_prep_type prep = (config_rsv) ?
					RESERV_PREP_TYPE_CHECK_IF_OCCUPIED :
					RESERV_PREP_TYPE_EXTEND_IF_NEEDED;

				ret = pmemset_prepare_reservation_range(
						pmem2_reserv, map_reserv_offset,
						part_size, prep);
				if (!ret) {
					pmap->desc.size += part_size;
					break;
				}

				if (coalescing == PMEMSET_COALESCING_FULL)
					break;
			}
		case PMEMSET_COALESCING_NONE:
			/* if reached this case, then don't coalesce */
			map_reserv_offset = 0;

			if (config_rsv) {
				pmem2_reserv = config_rsv;

				ret = pmemset_find_reservation_empty_range(
						pmem2_reserv, part_size,
						&map_reserv_offset);
			} else {
				ret = pmemset_create_reservation(&pmem2_reserv,
						part_size);
			}

			if (ret)
				break;

			ret = pmemset_part_map_new(&pmap, pmem2_reserv,
					map_reserv_offset, part_size);
			if (ret)
				goto err_adjust_vm_reserv;

			coalesced = false;
			break;
		default:
			ERR("invalid coalescing value %d", coalescing);
			ret = PMEMSET_E_INVALID_COALESCING_VALUE;
			goto err_lock_unlock;
	}

	if (ret) {
		if (ret == PMEMSET_E_PART_EXISTS) {
			ERR(
				"new part couldn't be coalesced with the previous part map %p "
				"the memory range after the previous mapped part is occupied",
					pmap);
			ret = PMEMSET_E_CANNOT_COALESCE_PARTS;
		} else if (ret == PMEMSET_E_LENGTH_UNALIGNED) {
			ERR(
				"part length for the mapping %zu is not a multiple of %llu",
					part_size, Mmap_align);
		}
		goto err_lock_unlock;
	}

	ASSERTne(pmap, NULL);

	pmem2_config_set_vm_reservation(pmem2_cfg, pmem2_reserv,
			map_reserv_offset);

	struct pmem2_map *pmem2_map;
	ret = pmem2_map_new(&pmem2_map, pmem2_cfg, pmem2_src);
	if (ret) {
		ERR("cannot create pmem2 mapping %d", ret);
		ret = PMEMSET_E_INVALID_PMEM2_MAP;
		goto err_pmap_revert;
	}

	/*
	 * effective granularity is only set once and
	 * must have the same value for each mapping
	 */
	bool effective_gran_valid = set->effective_granularity_valid;
	mapping_gran = pmem2_map_get_store_granularity(pmem2_map);

	if (effective_gran_valid == false) {
		pmemset_set_store_granularity(set, mapping_gran);
		set->effective_granularity_valid = true;
	} else {
		enum pmem2_granularity set_effective_gran;
		ret = pmemset_get_store_granularity(set, &set_effective_gran);
		ASSERTeq(ret, 0);

		if (set_effective_gran != mapping_gran) {
			ERR(
				"the part granularity is %s, all parts in the set must have the same granularity %s",
				granularity_name[mapping_gran],
				granularity_name[set_effective_gran]);
			ret = PMEMSET_E_GRANULARITY_MISMATCH;
			goto err_p2map_delete;
		}
	}

	pmemset_set_persisting_fn(set, pmap);
	pmemset_set_mem_fn(set, pmap);

	/* insert part map only if it is new */
	if (!coalesced) {
		ret = pmemset_insert_part_map(set, pmap);
		if (ret)
			goto err_p2map_delete;
		set->shared_state.previous_pmap = pmap;
	}

	/* pass the descriptor */
	if (desc)
		*desc = pmap->desc;

	/* consume the part */
	ret = pmemset_part_delete(part_ptr);
	ASSERTeq(ret, 0);
	/* delete temporary pmem2 config */
	ret = pmem2_config_delete(&pmem2_cfg);
	ASSERTeq(ret, 0);

	util_rwlock_unlock(&set->shared_state.lock);

	struct pmemset_event_part_add event;
	event.addr = pmap->desc.addr;
	event.len = pmap->desc.size;
	event.src = pmem2_src;

	struct pmemset_event_context ctx;
	ctx.type = PMEMSET_EVENT_PART_ADD;
	ctx.data.part_add = event;

	pmemset_config_event_callback(set_config, set, &ctx);

	return 0;

err_p2map_delete:
	pmem2_map_delete(&pmem2_map);
err_pmap_revert:
	if (coalesced)
		pmap->desc.size -= part_size;
	else
		pmemset_part_map_delete(&pmap);
err_adjust_vm_reserv:
	/* reservation provided by the user should not be modified */
	if (pmemset_config_get_reservation(set_config) == NULL)
		pmemset_adjust_reservation_to_contents(&pmem2_reserv);
err_lock_unlock:
	util_rwlock_unlock(&set->shared_state.lock);
err_pmem2_cfg_delete:
	pmem2_config_delete(&pmem2_cfg);
	return ret;
}

/*
 * pmemset_update_previous_part_map -- updates previous part map for the
 *                                     provided pmemset
 */
static void
pmemset_update_previous_part_map(struct pmemset *set,
		struct pmemset_part_map *pmap)
{
	struct ravl_interval_node *node;
	node = ravl_interval_find_prev(set->shared_state.part_map_tree,
			pmap);
	if (!node)
		node = ravl_interval_find_next(set->shared_state.part_map_tree,
				pmap);

	set->shared_state.previous_pmap = (node) ? ravl_interval_data(node) :
			NULL;
}

/*
 * pmemset_remove_part_map -- unmaps the part and removes it from the set
 */
int
pmemset_remove_part_map(struct pmemset *set, struct pmemset_part_map **pmap_ptr)
{
	LOG(3, "set %p part map %p", set, pmap_ptr);
	PMEMSET_ERR_CLR();

	struct pmemset_part_map *pmap = *pmap_ptr;

	if (pmap->refcount > 1) {
		ERR(
			"cannot delete part map with reference count %d, " \
			"part map must only be referenced once",
				pmap->refcount);
		return PMEMSET_E_PART_MAP_POSSIBLE_USE_AFTER_DROP;
	}

	util_rwlock_wrlock(&set->shared_state.lock);

	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;
	int ret = pmemset_unregister_part_map(set, pmap);
	if (ret)
		goto err_lock_unlock;

	/*
	 * if the part mapping to be removed is the same as the one being stored
	 * in the pmemset to map parts contiguously, then update it
	 */
	if (set->shared_state.previous_pmap == pmap)
		pmemset_update_previous_part_map(set, pmap);

	size_t pmap_size = pmemset_descriptor_part_map(pmap).size;
	/* delete all pmem2 maps contained in the part map */
	ret = pmemset_part_map_remove_range(pmap, 0, pmap_size, NULL, NULL);
	if (ret)
		goto err_insert_pmap;

	ret = pmemset_part_map_delete(pmap_ptr);
	if (ret)
		goto err_insert_pmap;

	/* reservation provided by the user should not be modified */
	if (pmemset_config_get_reservation(set->set_config) == NULL) {
		ret = pmemset_adjust_reservation_to_contents(&pmem2_reserv);
		ASSERTeq(ret, 0);
	}

	util_rwlock_unlock(&set->shared_state.lock);

	return 0;

err_insert_pmap:
	pmemset_insert_part_map(set, pmap);
err_lock_unlock:
	util_rwlock_unlock(&set->shared_state.lock);
	return ret;
}

/*
 * typedef for callback function invoked on each iteration of part mapping
 * stored in the pmemset
 */
typedef int pmemset_iter_cb(struct pmemset *set, struct pmemset_part_map *pmap,
		void *arg);

/*
 * pmemset_iterate -- iterates over every part mapping stored in the
 * pmemset overlapping with the region defined by the address and size.
 */
static int
pmemset_iterate(struct pmemset *set, void *addr, size_t len, pmemset_iter_cb cb,
		void *arg)
{
	size_t end_addr = (size_t)addr + len;

	struct pmemset_part_map dummy_map;
	dummy_map.desc.addr = addr;
	dummy_map.desc.size = len;
	struct ravl_interval_node *node = ravl_interval_find(
			set->shared_state.part_map_tree, &dummy_map);
	while (node) {
		struct pmemset_part_map *fmap = ravl_interval_data(node);
		size_t fmap_addr = (size_t)fmap->desc.addr;
		size_t fmap_size = fmap->desc.size;

		int ret = cb(set, fmap, arg);
		if (ret)
			return ret;

		size_t cur_addr = fmap_addr + fmap_size;
		if (end_addr > cur_addr) {
			dummy_map.desc.addr = (char *)cur_addr;
			dummy_map.desc.size = end_addr - cur_addr;
			node = ravl_interval_find(
					set->shared_state.part_map_tree,
					&dummy_map);
		} else {
			node = NULL;
		}
	}

	return 0;
}

struct pmap_remove_range_arg {
	size_t addr;
	size_t size;
};

/*
 * pmemset_remove_part_map_range_cb -- callback for removing part map range on
 *                                     each iteration
 */
static int
pmemset_remove_part_map_range_cb(struct pmemset *set,
		struct pmemset_part_map *pmap, void *arg)
{
	if (pmap->refcount > 0) {
		ERR(
			"cannot delete part map with reference count %d, " \
			"part maps residing at the provided range must not be referenced by any thread",
				pmap->refcount);
		return PMEMSET_E_PART_MAP_POSSIBLE_USE_AFTER_DROP;
	}

	struct pmap_remove_range_arg *rarg =
			(struct pmap_remove_range_arg *)arg;
	size_t rm_addr = rarg->addr;
	size_t rm_size = rarg->size;

	size_t pmap_addr = (size_t)pmap->desc.addr;
	size_t pmap_size = pmap->desc.size;
	struct pmem2_vm_reservation *pmem2_reserv = pmap->pmem2_reserv;

	/*
	 * If the remove range starting address is earlier than the part mapping
	 * address then the minimal possible offset is 0, if it's later then
	 * calculate the difference and set it as offset. Adjust the range size
	 * to match either of those cases.
	 */
	size_t rm_offset = (rm_addr > pmap_addr) ? rm_addr - pmap_addr : 0;
	size_t rm_size_adjusted = rm_addr + rm_size - pmap_addr - rm_offset;

	size_t true_rm_offset;
	size_t true_rm_size;
	int ret = pmemset_part_map_remove_range(pmap, rm_offset,
			rm_size_adjusted, &true_rm_offset, &true_rm_size);
	if (ret)
		return ret;

	/* none of those functions should fail */
	if (true_rm_offset == 0 && true_rm_size == pmap_size) {
		if (set->shared_state.previous_pmap == pmap)
			pmemset_update_previous_part_map(set, pmap);

		ret = pmemset_unregister_part_map(set, pmap);
		ASSERTeq(ret, 0);
		ret = pmemset_part_map_delete(&pmap);
		ASSERTeq(ret, 0);
	} else if (true_rm_offset == 0) {
		pmap->desc.addr = (char *)pmap->desc.addr + true_rm_size;
		pmap->desc.size -= true_rm_size;
	} else if (true_rm_offset + true_rm_size == pmap_size) {
		pmap->desc.size -= true_rm_size;
	} else {
		size_t rsv_addr = (size_t)pmem2_vm_reservation_get_address(
				pmem2_reserv);
		ASSERT(pmap_addr >= rsv_addr);
		size_t pmap_offset = pmap_addr - rsv_addr;

		size_t new_pmap_offset = true_rm_offset + true_rm_size;
		size_t new_pmap_size = pmap_offset + pmap_size -
				new_pmap_offset;

		struct pmemset_part_map *new_pmap;
		/* part map was severed into two part maps */
		ret = pmemset_part_map_new(&new_pmap, pmem2_reserv,
				new_pmap_offset, new_pmap_size);
		if (ret)
			return ret;

		pmap->desc.size = pmap_size - new_pmap_size - true_rm_size;

		ret = pmemset_insert_part_map(set, new_pmap);
		ASSERTeq(ret, 0);
	}

	struct pmemset_config *cfg = pmemset_get_pmemset_config(set);
	/* reservation provided by the user should not be modified */
	if (pmemset_config_get_reservation(cfg) == NULL) {
		ret = pmemset_adjust_reservation_to_contents(&pmem2_reserv);
		ASSERTeq(ret, 0);
	}

	return 0;
}

/*
 * pmemset_remove_range -- removes the file mappings covering the memory ranges
 *                         contained in or intersected with the provided range
 */
int
pmemset_remove_range(struct pmemset *set, void *addr, size_t len)
{
	LOG(3, "set %p addr %p len %zu", set, addr, len);
	PMEMSET_ERR_CLR();

	struct pmap_remove_range_arg arg;
	arg.addr = (size_t)addr;
	arg.size = len;

	util_rwlock_wrlock(&set->shared_state.lock);
	int ret = pmemset_iterate(set, addr, len,
			pmemset_remove_part_map_range_cb, &arg);
	util_rwlock_unlock(&set->shared_state.lock);

	return ret;
}

/*
 * pmemset_persist -- persists stores from provided range
 */
int
pmemset_persist(struct pmemset *set, const void *ptr, size_t size)
{
	LOG(15, "ptr %p size %zu", ptr, size);

	/*
	 * someday, for debug purposes, we can validate
	 * if ptr and size belongs to the set
	 */
	set->persist_fn(ptr, size);
	return 0;
}

/*
 * pmemset_flush -- flushes stores from passed range
 */
int
pmemset_flush(struct pmemset *set, const void *ptr, size_t size)
{
	LOG(15, "ptr %p size %zu", ptr, size);

	/*
	 * someday, for debug purposes, we can validate
	 * if ptr and size belongs to the set
	 */
	set->flush_fn(ptr, size);
	return 0;
}

/*
 * pmemset_drain -- drain stores
 */
int
pmemset_drain(struct pmemset *set)
{
	LOG(15, "set %p", set);

	set->drain_fn();
	return 0;
}

/*
 * pmemset_memmove -- memmove to pmemset dest
 */
void *
pmemset_memmove(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags)
{
	LOG(15, "set %p pmemdest %p src %p len %zu flags 0x%x",
			set, pmemdest, src, len, flags);

#ifdef DEBUG
	if (flags & ~PMEMSET_F_MEM_VALID_FLAGS)
		ERR("pmemset_memmove invalid flags 0x%x", flags);
#endif

	return set->memmove_fn(pmemdest, src, len, flags);
}

/*
 * pmemset_memcpy -- memcpy to pmemset
 */
void *
pmemset_memcpy(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags)
{
	LOG(15, "set %p pmemdest %p src %p len %zu flags 0x%x",
			set, pmemdest, src, len, flags);

#ifdef DEBUG
	if (flags & ~PMEMSET_F_MEM_VALID_FLAGS)
		ERR("pmemset_memcpy invalid flags 0x%x", flags);
#endif

	return set->memcpy_fn(pmemdest, src, len, flags);
}

/*
 * pmemset_memset -- memset pmemdest
 */
void *
pmemset_memset(struct pmemset *set, void *pmemdest, int c,
		size_t len, unsigned flags)
{
	LOG(15, "set %p pmemdest %p c %d len %zu flags 0x%x",
			set, pmemdest, c, len, flags);

#ifdef DEBUG
	if (flags & ~PMEMSET_F_MEM_VALID_FLAGS)
		ERR("pmemset_memset invalid flags 0x%x", flags);
#endif

	return set->memset_fn(pmemdest, c, len, flags);
}

/*
 * deep_flush_pmem2_maps_from_rsv -- perform pmem2 deep flush
 * for each pmem2_map from reservation from range.
 * This function sets *end* param to true if in the reservation
 * is last pmem2 map from the provided pmemset_deep_flush range
 * or the reservation end arrder is gt than range and addr.
 */
static int
deep_flush_pmem2_maps_from_rsv(struct pmem2_vm_reservation *rsv,
		char *range_ptr, char *range_end, bool *end)
{
	int ret = 0;
	struct pmem2_map *map;
	size_t rsv_len = pmem2_vm_reservation_get_size(rsv);
	char *rsv_addr = pmem2_vm_reservation_get_address(rsv);
	size_t off = 0;
	char *map_addr;
	char *map_end;
	size_t map_size;
	char *flush_addr;
	size_t flush_size;
	size_t len = rsv_len;
	*end = false;

	while (*end == false && ret == 0) {
		ret = pmem2_vm_reservation_map_find(rsv, off, len, &map);
		if (ret == PMEM2_E_MAPPING_NOT_FOUND) {
			ret = 0;
			if (range_end <= rsv_addr + rsv_len)
				*end = true;
			break;
		}

		map_size = pmem2_map_get_size(map);
		map_addr = pmem2_map_get_address(map);
		map_end = map_addr + map_size;

		flush_addr = map_addr;
		flush_size = map_size;

		if (range_end <= map_addr) {
			*end = true;
			break;
		}
		if (range_ptr >= map_end)
			goto next;

		if (range_ptr >= map_addr && range_ptr < map_end)
			flush_addr = range_ptr;

		if (range_end <= map_end) {
			flush_size = (size_t)(range_end - flush_addr);
			*end = true;
		} else {
			flush_size = (size_t)(map_end - flush_addr);
		}

		ret = pmem2_deep_flush(map, flush_addr, flush_size);
		if (ret) {
			ERR("cannot perform deep flush on the reservation");
			ret = PMEMSET_E_DEEP_FLUSH_FAIL;
		}
	next:
		off = (size_t)(map_end - rsv_addr);
		len = rsv_len - off;
	}

	return ret;
}

/*
 * pmemset_deep_flush -- perform deep flush operation
 */
int
pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size)
{
	LOG(3, "set %p ptr %p size %zu", set, ptr, size);
	PMEMSET_ERR_CLR();

	struct pmemset_part_map *pmap = NULL;
	struct pmemset_part_map *next_pmap = NULL;

	int ret = pmemset_part_map_by_address(set, &pmap, ptr);
	if (ret == PMEMSET_E_CANNOT_FIND_PART_MAP) {
		struct pmemset_part_map cur;
		cur.desc.addr = ptr;
		cur.desc.size = 1;

		pmemset_next_part_map(set, &cur, &next_pmap);

		if (!next_pmap)
			return 0;
		pmap = next_pmap;
	}

	struct pmem2_vm_reservation *rsv = pmap->pmem2_reserv;
	char *range_end = (char *)ptr + size;
	void *rsv_addr;
	bool end;
	ret = 0;

	while (rsv) {
		rsv_addr = pmem2_vm_reservation_get_address(rsv);
		if ((char *)rsv_addr > range_end)
			break;

		ret = deep_flush_pmem2_maps_from_rsv(rsv, (char *)ptr,
				range_end, &end);
		if (ret || end)
			break;

		pmemset_next_part_map(set, pmap, &next_pmap);
		if (next_pmap == NULL)
			break;

		rsv = next_pmap->pmem2_reserv;
	}

	return ret;
}

/*
 * pmemset_get_pmemset_config -- get pmemset config
 */
struct pmemset_config *
pmemset_get_pmemset_config(struct pmemset *set)
{
	LOG(3, "%p", set);
	return set->set_config;
}

/*
 * pmemset_part_map_access -- gains access to the part mapping
 */
static void
pmemset_part_map_access(struct pmemset_part_map *pmap)
{
	pmap->refcount += 1;
}

/*
 * pmemset_part_map_access_drop -- drops the access to the part mapping
 */
static void
pmemset_part_map_access_drop(struct pmemset_part_map *pmap)
{
	pmap->refcount -= 1;
	ASSERT(pmap->refcount >= 0);
}

/*
 * pmemset_first_part_map -- retrieve first part map from the set
 */
void
pmemset_first_part_map(struct pmemset *set, struct pmemset_part_map **pmap)
{
	LOG(3, "set %p pmap %p", set, pmap);
	PMEMSET_ERR_CLR();

	*pmap = NULL;

	util_rwlock_rdlock(&set->shared_state.lock);

	struct ravl_interval_node *node = ravl_interval_find_first(
			set->shared_state.part_map_tree);

	if (node) {
		*pmap = ravl_interval_data(node);
		pmemset_part_map_access(*pmap);
	}

	util_rwlock_unlock(&set->shared_state.lock);
}

/*
 * pmemset_next_part_map -- retrieve successor part map in the set
 */
void
pmemset_next_part_map(struct pmemset *set, struct pmemset_part_map *cur,
		struct pmemset_part_map **next)
{
	LOG(3, "set %p cur %p next %p", set, cur, next);
	PMEMSET_ERR_CLR();

	*next = NULL;

	util_rwlock_rdlock(&set->shared_state.lock);

	struct ravl_interval_node *node = ravl_interval_find_next(
			set->shared_state.part_map_tree, cur);

	if (node) {
		*next = ravl_interval_data(node);
		pmemset_part_map_access(*next);
	}

	util_rwlock_unlock(&set->shared_state.lock);
}

/*
 * pmemset_part_map_by_address -- returns part map by passed address
 */
int
pmemset_part_map_by_address(struct pmemset *set, struct pmemset_part_map **pmap,
		void *addr)
{
	LOG(3, "set %p pmap %p addr %p", set, pmap, addr);
	PMEMSET_ERR_CLR();

	*pmap = NULL;
	int ret = 0;

	struct pmemset_part_map ppm;
	ppm.desc.addr = addr;
	ppm.desc.size = 1;

	util_rwlock_rdlock(&set->shared_state.lock);

	struct ravl_interval_node *node;
	node = ravl_interval_find(set->shared_state.part_map_tree, &ppm);

	if (!node) {
		ERR("cannot find part_map at addr %p in the set %p", addr, set);
		ret = PMEMSET_E_CANNOT_FIND_PART_MAP;
		goto err_lock_unlock;
	}

	*pmap = (struct pmemset_part_map *)ravl_interval_data(node);
	pmemset_part_map_access(*pmap);

err_lock_unlock:
	util_rwlock_unlock(&set->shared_state.lock);

	return ret;
}

/*
 * pmemset_map_descriptor -- create and return a part map descriptor
 */
struct pmemset_part_descriptor
pmemset_descriptor_part_map(struct pmemset_part_map *pmap)
{
	return pmap->desc;
}

/*
 * pmemset_part_map_drop -- drops the reference to the part map through provided
 *                          pointer. Doesn't delete part map.
 */
void
pmemset_part_map_drop(struct pmemset_part_map **pmap)
{
	LOG(3, "pmap %p", pmap);

	pmemset_part_map_access_drop(*pmap);
	*pmap = NULL;
}

/*
 * pmemset_set_contiguous_part_coalescing -- sets the part coalescing feature
 *                                           in the provided set on or off
 */
int
pmemset_set_contiguous_part_coalescing(struct pmemset *set,
		enum pmemset_coalescing value)
{
	LOG(3, "set %p coalescing %d", set, value);

	switch (value) {
		case PMEMSET_COALESCING_NONE:
		case PMEMSET_COALESCING_OPPORTUNISTIC:
		case PMEMSET_COALESCING_FULL:
			break;
		default:
			ERR("invalid coalescing value %d", value);
			return PMEMSET_E_INVALID_COALESCING_VALUE;
	}
	set->part_coalescing = value;

	return 0;
}
