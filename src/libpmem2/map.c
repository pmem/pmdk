// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * map.c -- pmem2_map (common)
 */

#include "out.h"

#include "config.h"
#include "map.h"
#include "os.h"
#include "os_thread.h"
#include "pmem2.h"
#include "pmem2_utils.h"
#include "ravl.h"
#include "sys_util.h"

#include <libpmem2.h>

/*
 * pmem2_map_get_address -- get mapping address
 */
void *
pmem2_map_get_address(struct pmem2_map *map)
{
	LOG(3, "map %p", map);

	return map->addr;
}

/*
 * pmem2_map_get_size -- get mapping size
 */
size_t
pmem2_map_get_size(struct pmem2_map *map)
{
	LOG(3, "map %p", map);

	return map->content_length;
}

/*
 * pmem2_map_get_store_granularity -- returns granularity of the mapped
 * file
 */
enum pmem2_granularity
pmem2_map_get_store_granularity(struct pmem2_map *map)
{
	LOG(3, "map %p", map);

	return map->effective_granularity;
}

/*
 * parse_force_granularity -- parse PMEM2_FORCE_GRANULARITY environment variable
 */
static enum pmem2_granularity
parse_force_granularity()
{
	char *ptr = os_getenv("PMEM2_FORCE_GRANULARITY");
	if (ptr) {
		char str[11]; /* strlen("CACHE_LINE") + 1 */

		if (util_safe_strcpy(str, ptr, sizeof(str))) {
			LOG(1, "Invalid value of PMEM2_FORCE_GRANULARITY");
			return PMEM2_GRANULARITY_INVALID;
		}

		char *s = str;
		while (*s) {
			*s = (char)toupper((char)*s);
			s++;
		}

		if (strcmp(str, "BYTE") == 0) {
			return PMEM2_GRANULARITY_BYTE;
		} else if (strcmp(str, "CACHE_LINE") == 0) {
			return PMEM2_GRANULARITY_CACHE_LINE;
		} else if (strcmp(str, "CACHELINE") == 0) {
			return PMEM2_GRANULARITY_CACHE_LINE;
		} else if (strcmp(str, "PAGE") == 0) {
			return PMEM2_GRANULARITY_PAGE;
		}

		LOG(1, "Invalid value of PMEM2_FORCE_GRANULARITY");
	}
	return PMEM2_GRANULARITY_INVALID;
}

/*
 * get_min_granularity -- checks min available granularity
 */
enum pmem2_granularity
get_min_granularity(bool eADR, bool is_pmem, enum pmem2_sharing_type sharing)
{
	enum pmem2_granularity force = parse_force_granularity();
	/* PMEM2_PRIVATE sharing does not require data flushing */
	if (sharing == PMEM2_PRIVATE)
		return PMEM2_GRANULARITY_BYTE;
	if (force != PMEM2_GRANULARITY_INVALID)
		return force;
	if (!is_pmem)
		return PMEM2_GRANULARITY_PAGE;
	if (!eADR)
		return PMEM2_GRANULARITY_CACHE_LINE;

	return PMEM2_GRANULARITY_BYTE;
}

/*
 * pmem2_validate_offset -- verify if the offset is a multiple of
 * the alignment required for the config
 */
int
pmem2_validate_offset(const struct pmem2_config *cfg, size_t *offset,
	size_t alignment)
{
	ASSERTne(alignment, 0);
	if (cfg->offset % alignment) {
		ERR("offset is not a multiple of %lu", alignment);
		return PMEM2_E_OFFSET_UNALIGNED;
	}

	*offset = cfg->offset;

	return 0;
}

static struct ravl *Mappings;
static os_rwlock_t Mappings_lock;

/*
 * mappings_compare -- compare pmem2_maps by starting address
 */
static int
mappings_compare(const void *lhs, const void *rhs)
{
	const struct pmem2_map *l = lhs;
	const struct pmem2_map *r = rhs;

	if (l->addr < r->addr)
		return -1;
	if (l->addr > r->addr)
		return 1;
	return 0;
}

/*
 * pmem2_map_init -- initialize the map module
 */
void
pmem2_map_init(void)
{
	os_rwlock_init(&Mappings_lock);
	Mappings = ravl_new(mappings_compare);
	if (!Mappings)
		abort();
}

/*
 * pmem2_map_fini -- finalize the map module
 */
void
pmem2_map_fini(void)
{
	ravl_delete(Mappings);
	Mappings = NULL;
	os_rwlock_destroy(&Mappings_lock);
}

/*
 * pmem2_register_mapping -- register mapping in the mappings tree
 */
int
pmem2_register_mapping(struct pmem2_map *map)
{
	int ret;

	util_rwlock_wrlock(&Mappings_lock);
	ret = ravl_insert(Mappings, map);
	util_rwlock_unlock(&Mappings_lock);

	if (ret)
		return PMEM2_E_ERRNO;

	return 0;
}

/*
 * pmem2_unregister_mapping -- unregister mapping from the mappings tree
 */
int
pmem2_unregister_mapping(struct pmem2_map *map)
{
	int ret = 0;
	util_rwlock_wrlock(&Mappings_lock);
	struct ravl_node *n = ravl_find(Mappings, map, RAVL_PREDICATE_EQUAL);
	if (n)
		ravl_remove(Mappings, n);
	else
		ret = PMEM2_E_MAPPING_NOT_FOUND;
	util_rwlock_unlock(&Mappings_lock);

	return ret;
}

/*
 * pmem2_map_find_prior_or_eq -- find overlapping mapping starting prior to
 * the current one or at the same address
 */
static struct pmem2_map *
pmem2_map_find_prior_or_eq(struct pmem2_map *cur)
{
	struct ravl_node *n;
	struct pmem2_map *map;

	n = ravl_find(Mappings, cur, RAVL_PREDICATE_LESS_EQUAL);
	if (!n)
		return NULL;

	map = ravl_data(n);

	/*
	 * If the end of the found mapping is below the searched address, then
	 * this is not our mapping.
	 */
	if ((char *)map->addr + map->reserved_length <= (char *)cur->addr)
		return NULL;

	return map;
}

/*
 * pmem2_map_find_later -- find overlapping mapping starting later than
 * the current one
 */
static struct pmem2_map *
pmem2_map_find_later(struct pmem2_map *cur)
{
	struct ravl_node *n;
	struct pmem2_map *map;

	n = ravl_find(Mappings, cur, RAVL_PREDICATE_GREATER);
	if (!n)
		return NULL;

	map = ravl_data(n);

	/*
	 * If the beginning of the found mapping is above the end of
	 * the searched range, then this is not our mapping.
	 */
	if ((char *)map->addr >= (char *)cur->addr + cur->reserved_length)
		return NULL;

	return map;
}

/*
 * pmem2_map_find -- find the earliest mapping overlapping with
 * [addr, addr+size) range
 */
struct pmem2_map *
pmem2_map_find(const void *addr, size_t len)
{
	struct pmem2_map cur;
	struct pmem2_map *map;

	util_rwlock_rdlock(&Mappings_lock);

	cur.addr = (void *)addr;
	cur.reserved_length = len;

	map = pmem2_map_find_prior_or_eq(&cur);
	if (!map)
		map = pmem2_map_find_later(&cur);

	util_rwlock_unlock(&Mappings_lock);

	return map;
}
