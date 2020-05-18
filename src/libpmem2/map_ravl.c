// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * map_ravl.c -- map_ravl implementation
 */

#include "alloc.h"
#include "map.h"
#include "map_ravl.h"
#include "pmem2_utils.h"
#include "sys_util.h"
#include "os_thread.h"
#include "ravl.h"

/*
 * map_ravl - structure holding ravl tree and a lock
 */
struct map_ravl {
	struct ravl *tree;
	os_rwlock_t lock;
};

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
 * map_ravl_delete - finalize the map module
 */
void
map_ravl_delete(struct map_ravl **mr)
{
	ravl_delete((*mr)->tree);
	(*mr)->tree = NULL;
	os_rwlock_destroy(&((*mr)->lock));
	Free(*mr);
}

/*
 * map_ravl_new -- initialize the map module
 */
int
map_ravl_new(struct map_ravl **mr)
{
	int ret;
	*mr = pmem2_malloc(sizeof(struct map_ravl), &ret);

	if (ret)
		return ret;

	os_rwlock_init(&((*mr)->lock));
	(*mr)->tree = ravl_new(mappings_compare);

	if (!(*mr)->tree)
		return PMEM2_E_ERRNO;

	return 0;
}

/*
 * map_ravl_add -- add mapping to the mappings tree
 */
int
map_ravl_add(struct map_ravl **mr, struct pmem2_map *map)
{
	int ret;

	util_rwlock_wrlock(&((*mr)->lock));
	ret = ravl_insert((*mr)->tree, map);
	util_rwlock_unlock(&((*mr)->lock));

	if (ret)
		return PMEM2_E_ERRNO;

	return 0;
}

/*
 * map_ravl_remove -- remove mapping from the mappings tree
 */
int
map_ravl_remove(struct map_ravl **mr, struct pmem2_map *map)
{
	int ret = 0;

	util_rwlock_wrlock(&((*mr)->lock));
	struct ravl_node *n = ravl_find((*mr)->tree, map, RAVL_PREDICATE_EQUAL);

	if (n)
		ravl_remove((*mr)->tree, n);
	else
		ret = PMEM2_E_MAPPING_NOT_FOUND;

	util_rwlock_unlock(&((*mr)->lock));

	return ret;
}

/*
 * map_ravl_find_prior_or_eq -- find overlapping mapping starting prior to
 * the current one or at the same address
 */
static struct pmem2_map *
map_ravl_find_prior_or_eq(struct map_ravl **mr, struct pmem2_map *cur)
{
	struct ravl_node *n;
	struct pmem2_map *map;

	n = ravl_find((*mr)->tree, cur, RAVL_PREDICATE_LESS_EQUAL);
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
 * map_ravl_find_later -- find overlapping mapping starting later than
 * the current one
 */
static struct pmem2_map *
map_ravl_find_later(struct map_ravl **mr, struct pmem2_map *cur)
{
	struct ravl_node *n;
	struct pmem2_map *map;

	n = ravl_find((*mr)->tree, cur, RAVL_PREDICATE_GREATER);
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
 * map_ravl_find -- find the earliest mapping overlapping with
 * (addr, addr + size) range
 */
struct pmem2_map *
map_ravl_find(struct map_ravl **mr, const void *addr, size_t size)
{
	struct pmem2_map cur;
	struct pmem2_map *map;

	util_rwlock_rdlock(&((*mr)->lock));

	cur.addr = (void *)addr;
	cur.reserved_length = size;

	map = map_ravl_find_prior_or_eq(mr, &cur);
	if (!map)
		map = map_ravl_find_later(mr, &cur);

	util_rwlock_unlock(&((*mr)->lock));

	return map;
}
