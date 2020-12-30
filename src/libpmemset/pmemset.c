// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset.c -- implementation of common pmemset API
 */

#include <stdbool.h>
#include "pmemset.h"
#include "libpmemset.h"
#include "libpmem2.h"
#include "part.h"
#include "pmemset_utils.h"
#include "ravl_interval.h"
#include "ravl.h"
#include "alloc.h"
#include "config.h"

/*
 * pmemset
 */
struct pmemset {
	struct pmemset_config *set_config;
	struct ravl_interval *part_map_tree;
	bool effective_granularity_valid;
	enum pmem2_granularity effective_granularity;
};

static char *granularity_name[3] = {
	"PMEM2_GRANULARITY_BYTE",
	"PMEM2_GRANULARITY_CACHE_LINE",
	"PMEM2_GRANULARITY_PAGE"
};

/*
 * pmemset_header
 */
struct pmemset_header {
	char stub;
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
	void *map_addr = pmap->desc.addr;
	size_t map_size = pmap->desc.size;
	return (size_t)map_addr + map_size;
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

	/* intialize RAVL */
	set->part_map_tree = ravl_interval_new(pmemset_mapping_min,
						pmemset_mapping_max);

	if (set->part_map_tree == NULL) {
		ERR("ravl tree initialization failed");
		return PMEMSET_E_ERRNO;
	}

	set->effective_granularity_valid = false;

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
 * pmemset_delete -- de-allocate set structure
 */
int
pmemset_delete(struct pmemset **set)
{
	LOG(3, "pmemset %p", set);
	PMEMSET_ERR_CLR();

	if (*set == NULL)
		return 0;

	struct ravl_interval_node *node;

	node = ravl_interval_find((*set)->part_map_tree, NULL);
	if (node) {
		struct pmemset_part_map *map = ravl_interval_data(node);
		ravl_interval_remove((*set)->part_map_tree, node);
		pmem2_map_delete(&map->pmem2_map);
		Free(map);
	}

	/* delete RAVL tree with part_map nodes */
	ravl_interval_delete((*set)->part_map_tree);

	/* delete cfg */
	pmemset_config_delete(&(*set)->set_config);

	Free(*set);

	*set = NULL;

	return 0;
}

/*
 * pmemset_insert_part_map -- insert part mapping into the set
 */
static int
pmemset_insert_part_map(struct pmemset *set, struct pmemset_part_map *map)
{
	int ret = ravl_interval_insert(set->part_map_tree, map);
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
 * pmemset_part_map -- create new part mapping and add it to the set
 */
int
pmemset_part_map(struct pmemset_part **part, struct pmemset_extras *extra,
		struct pmemset_part_descriptor *desc)
{
	LOG(3, "part %p extra %p desc %p", part, extra, desc);
	PMEMSET_ERR_CLR();

	struct pmemset_part *p = *part;
	struct pmemset_part_map *part_map;
	struct pmemset *set = pmemset_part_get_pmemset(p);
	struct pmemset_config *set_config = pmemset_get_pmemset_config(set);
	enum pmem2_granularity mapping_gran;
	enum pmem2_granularity config_gran =
		pmemset_get_config_granularity(set_config);

	int ret = pmemset_part_create_part_mapping(&part_map, p,
			config_gran, &mapping_gran);
	if (ret)
		return ret;

	/*
	 * effective granularity is only set once and
	 * must have the same value for each mapping
	 */

	bool effective_gran_valid = set->effective_granularity_valid;

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
			goto err_delete_pmap;
		}
	}

	/*
	 * XXX: add multiple part support
	 */
	ret = pmemset_insert_part_map(set, part_map);
	if (ret)
		goto err_delete_pmap;

	if (desc)
		*desc = part_map->desc;

	pmemset_part_delete(part);

	return 0;

err_delete_pmap:
	pmemset_part_delete_part_mapping(&part_map);
	return ret;
}

#ifndef _WIN32
/*
 * pmemset_header_init -- not supported
 */
int
pmemset_header_init(struct pmemset_header *header, const char *layout,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
}
#else
/*
 * pmemset_header_initU -- not supported
 */
int
pmemset_header_initU(struct pmemset_header *header, const char *layout,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_header_initW -- not supported
 */
int
pmemset_header_initW(struct pmemset_header *header, const wchar_t *layout,
		int major, int minor)
{
	return PMEMSET_E_NOSUPP;
}
#endif

/*
 * pmemset_remove_part -- not supported
 */
int
pmemset_remove_part(struct pmemset *set, struct pmemset_part **part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_remove_part_map -- not supported
 */
int
pmemset_remove_part_map(struct pmemset *set, struct pmemset_part_map **part)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_remove_range -- not supported
 */
int
pmemset_remove_range(struct pmemset *set, void *addr, size_t len)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_persist -- not supported
 */
int
pmemset_persist(struct pmemset *set, const void *ptr, size_t size)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_flush -- not supported
 */
int
pmemset_flush(struct pmemset *set, const void *ptr, size_t size)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_drain -- not supported
 */
int
pmemset_drain(struct pmemset *set)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_memmove -- not supported
 */
int
pmemset_memmove(struct pmemset *set, void *pmemdest, const void *src,
		size_t len, unsigned flags)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_memcpy -- not supported
 */
int
pmemset_memcpy(struct pmemset *set, void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_memset -- not supported
 */
int
pmemset_memset(struct pmemset *set, void *pmemdest, int c, size_t len,
		unsigned flags)
{
	return PMEMSET_E_NOSUPP;
}

/*
 * pmemset_deep_flush -- not supported
 */
int
pmemset_deep_flush(struct pmemset *set, void *ptr, size_t size)
{
	return PMEMSET_E_NOSUPP;
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
 * pmemset_map_first -- retrieve first part map from the set
 */
void
pmemset_first_part_map(struct pmemset *set, struct pmemset_part_map **pmap)
{
	LOG(3, "set %p pmap %p", set, pmap);
	PMEMSET_ERR_CLR();

	*pmap = NULL;

	struct ravl_interval_node *first = ravl_interval_find_first(
			set->part_map_tree);

	if (first)
		*pmap = ravl_interval_data(first);
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

	struct ravl_interval_node *found = ravl_interval_find_next(
			set->part_map_tree, cur);

	if (found)
		*next = ravl_interval_data(found);

}

/*
 * pmemset_map_descriptor -- create and return a part map descriptor
 */
struct pmemset_part_descriptor
pmemset_descriptor_part_map(struct pmemset_part_map *pmap)
{
	struct pmemset_part_descriptor desc;
	desc.addr = pmem2_map_get_address(pmap->pmem2_map);
	desc.size = pmem2_map_get_size(pmap->pmem2_map);

	return desc;
}
