// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset.c -- implementation of common pmemset API
 */

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
	struct pmemset_config *config;
	struct ravl_interval *part_map_tree;
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
	struct pmemset_part_map *pmap = (struct pmemset_part_map *)addr;
	return (size_t)pmem2_map_get_address(pmap->pmem2_map);
}

/*
 * pmemset_mapping_max
 */
static size_t
pmemset_mapping_max(void *addr)
{
	struct pmemset_part_map *pmap = (struct pmemset_part_map *)addr;
	void *map_addr = pmem2_map_get_address(pmap->pmem2_map);
	size_t map_size = pmem2_map_get_size(pmap->pmem2_map);
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
	set->config = NULL;
	ret = pmemset_config_duplicate(&set->config, config);
	if (ret)
		return ret;

	/* intialize RAVL */
	set->part_map_tree = ravl_interval_new(pmemset_mapping_min,
						pmemset_mapping_max);

	if (set->part_map_tree == NULL) {
		ERR("ravl tree initialization failed");
		return PMEMSET_E_ERRNO;
	}

	return 0;
}

/*
 * pmemset_new -- allocates and initialize pmemset structure.
 */
int
pmemset_new(struct pmemset **set, struct pmemset_config *cfg)
{
	PMEMSET_ERR_CLR();

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
	if (*set == NULL)
		return 0;

	/* delete RAVL tree with part_map nodes */
	ravl_interval_delete((*set)->part_map_tree);

	/* delete cfg */
	pmemset_config_delete(&(*set)->config);

	Free(*set);

	*set = NULL;

	return 0;
}

/*
 * pmemset_get_part_map_tree -- return part map tree from pmemset
 */
struct ravl_interval *
pmemset_get_part_map_tree(struct pmemset *set)
{
	return set->part_map_tree;
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
