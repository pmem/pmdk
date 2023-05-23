// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * map.c -- pmem2_map (common)
 */

#include "out.h"

#include "alloc.h"
#include "config.h"
#include "map.h"
#include "mover.h"
#include "os.h"
#include "os_thread.h"
#include "persist.h"
#include "pmem2.h"
#include "pmem2_utils.h"
#include "ravl.h"
#include "ravl_interval.h"
#include "sys_util.h"
#include "valgrind_internal.h"

#include <libpmem2.h>

/*
 * pmem2_map_get_address -- get mapping address
 */
void *
pmem2_map_get_address(struct pmem2_map *map)
{
	LOG(3, "map %p", map);

	/* we do not need to clear err because this function cannot fail */
	return map->addr;
}

/*
 * pmem2_map_get_size -- get mapping size
 */
size_t
pmem2_map_get_size(struct pmem2_map *map)
{
	LOG(3, "map %p", map);

	/* we do not need to clear err because this function cannot fail */
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

	/* we do not need to clear err because this function cannot fail */
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

/*
 * mapping_min - return min boundary for mapping
 */
static size_t
mapping_min(void *addr)
{
	struct pmem2_map *map = (struct pmem2_map *)addr;
	return (size_t)map->addr;
}

/*
 * mapping_max - return max boundary for mapping
 */
static size_t
mapping_max(void *addr)
{
	struct pmem2_map *map = (struct pmem2_map *)addr;
	return (size_t)map->addr + map->content_length;
}

static struct pmem2_state {
	struct ravl_interval *range_map;
	os_rwlock_t range_map_lock;
} State;

/*
 * pmem2_map_init -- initialize the map module
 */
void
pmem2_map_init()
{
	util_rwlock_init(&State.range_map_lock);

	util_rwlock_wrlock(&State.range_map_lock);
	State.range_map = ravl_interval_new(mapping_min, mapping_max);
	util_rwlock_unlock(&State.range_map_lock);

	if (!State.range_map)
		abort();
}

/*
 * pmem2_map_fini -- finalize the map module
 */
void
pmem2_map_fini(void)
{
	util_rwlock_wrlock(&State.range_map_lock);
	ravl_interval_delete(State.range_map);
	util_rwlock_unlock(&State.range_map_lock);

	util_rwlock_destroy(&State.range_map_lock);
}

/*
 * pmem2_register_mapping -- register mapping in the mappings tree
 */
int
pmem2_register_mapping(struct pmem2_map *map)
{
	util_rwlock_wrlock(&State.range_map_lock);
	int ret = ravl_interval_insert(State.range_map, map);
	util_rwlock_unlock(&State.range_map_lock);

	return ret;
}

/*
 * pmem2_unregister_mapping -- unregister mapping from the mappings tree
 */
int
pmem2_unregister_mapping(struct pmem2_map *map)
{
	int ret = 0;
	struct ravl_interval_node *node;

	util_rwlock_wrlock(&State.range_map_lock);
	node = ravl_interval_find_equal(State.range_map, map);
	if (!(node && !ravl_interval_remove(State.range_map, node))) {
		ERR("Cannot find mapping %p to delete", map);
		ret = PMEM2_E_MAPPING_NOT_FOUND;
	}

	util_rwlock_unlock(&State.range_map_lock);

	return ret;
}

/*
 * pmem2_map_find -- find the earliest mapping overlapping with
 * (addr, addr+size) range
 */
struct pmem2_map *
pmem2_map_find(const void *addr, size_t len)
{
	struct pmem2_map map;
	map.addr = (void *)addr;
	map.content_length = len;

	struct ravl_interval_node *node;

	util_rwlock_rdlock(&State.range_map_lock);
	node = ravl_interval_find(State.range_map, &map);
	util_rwlock_unlock(&State.range_map_lock);

	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}

/*
 * pmem2_map_from_existing -- create map object for existing mapping
 */
int
pmem2_map_from_existing(struct pmem2_map **map_ptr,
	const struct pmem2_source *src, void *addr, size_t len,
	enum pmem2_granularity gran)
{
	int ret;
	struct pmem2_map *map =
		(struct pmem2_map *)pmem2_malloc(sizeof(*map), &ret);

	if (!map)
		return ret;

	map->reserv = NULL;
	map->addr = addr;
	map->reserved_length = 0;
	map->content_length = len;
	map->effective_granularity = gran;
	pmem2_set_flush_fns(map);
	pmem2_set_mem_fns(map);
	map->source = *src;

	/* XXX: there is no way to set custom vdm in this function */
	ret = mover_new(map, &map->vdm);
	if (ret) {
		goto err_map;
	}
	map->custom_vdm = false;

#ifndef _WIN32
	/* fd should not be used after map */
	map->source.value.fd = INVALID_FD;
#endif
	ret = pmem2_register_mapping(map);
	if (ret) {
		if (ret == -EEXIST) {
			ERR(
				"Provided mapping(addr %p len %zu) is already registered by libpmem2",
				addr, len);
			ret = PMEM2_E_MAP_EXISTS;
		}
		goto err_vdm;
	}
#ifndef _WIN32
	if (src->type == PMEM2_SOURCE_FD) {
		VALGRIND_REGISTER_PMEM_MAPPING(map->addr,
			map->content_length);
	}
#endif
	*map_ptr = map;
	return 0;
err_vdm:
	mover_delete(map->vdm);
err_map:
	Free(map);

	return ret;
}
