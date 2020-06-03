// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * vm_reservation.c -- implementation of virtual memory allocation API
 */

#include "alloc.h"
#include "map.h"
#include "pmem2_utils.h"
#include "ravl_interval.h"
#include "sys_util.h"
#include "vm_reservation.h"

/*
 * pmem2_vm_reservation_get_address -- get reservation address
 */
void *
pmem2_vm_reservation_get_address(struct pmem2_vm_reservation *rsv)
{
	LOG(3, "reservation %p", rsv);

	return rsv->addr;
}

/*
 * pmem2_vm_reservation_get_size -- get reservation size
 */
size_t
pmem2_vm_reservation_get_size(struct pmem2_vm_reservation *rsv)
{
	LOG(3, "reservation %p", rsv);

	return rsv->size;
}

/*
 * pmem2_vm_reservation_new -- creates new virtual memory reservation
 */
int
pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv_ptr,
	void *addr, size_t size)
{
	if (addr && (long long unsigned)addr % Pagesize) {
		ERR("address %p is not a multiple of pagesize 0x%llx", addr,
			Pagesize);
		return PMEM2_E_ADDRESS_UNALIGNED;
	}

	if (size % Pagesize) {
		ERR("reservation size %zu is not a multiple of pagesize %llu",
			size, Pagesize);
		return PMEM2_E_LENGTH_UNALIGNED;
	}

	int ret;
	(*rsv_ptr) = pmem2_malloc(sizeof(struct pmem2_vm_reservation), &ret);
	if (ret) {
		return ret;
	}

	struct pmem2_vm_reservation *rsv = *rsv_ptr;

	/* initialize the ravl interval tree */
	vm_reservation_init(rsv);

	void *raddr = NULL;
	size_t rsize = 0;
	ret = memory_reserve(addr, size, &raddr, &rsize);
	if (ret)
		goto err_reserve;

	rsv->addr = raddr;
	rsv->size = rsize;

	return 0;

err_reserve:
	vm_reservation_fini(rsv);
	Free(rsv);
	return ret;
}

/*
 * mapping_min - return min boundary for mapping
 */
static size_t
mapping_min(void *map)
{
	return (size_t)pmem2_map_get_address(map);
}

/*
 * mapping_max - return max boundary for mapping
 */
static size_t
mapping_max(void *map)
{
	return (size_t)pmem2_map_get_address(map) +
		pmem2_map_get_size(map);
}

/*
 * pmem2_vm_reservation_init - initialize the reservation structure
 */
void
vm_reservation_init(struct pmem2_vm_reservation *rsv)
{
	rsv->itree = ravl_interval_new(mapping_min, mapping_max);

	if (!rsv->itree)
		abort();
}

void
vm_reservation_fini(struct pmem2_vm_reservation *rsv)
{
	ravl_interval_delete(rsv->itree);
}

/*
 * vm_reservation_map_register -- register mapping in the mappings tree
 *                                of reservation structure
 */
int
vm_reservation_map_register(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map)
{
	return ravl_interval_insert(rsv->itree, map);
}

/*
 * vm_reservation_map_unregister -- unregister mapping from the mapping tree
 *                                  of reservation structure
 */
int
vm_reservation_map_unregister(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map)
{
	int ret = 0;
	struct ravl_interval_node *node;

	node = ravl_interval_find_equal(rsv->itree, map);
	if (node)
		ret = ravl_interval_remove(rsv->itree, node);
	else
		ret = PMEM2_E_MAPPING_NOT_FOUND;

	return ret;
}

/*
 * vm_reservation_map_find -- find the earliest mapping overlapping with
 *                            (addr, addr+size) range
 */
struct pmem2_map *
vm_reservation_map_find(struct pmem2_vm_reservation *rsv, size_t reserv_offset,
		size_t len)
{
	struct pmem2_map map;
	map.addr = (char *)rsv->addr + reserv_offset;
	map.content_length = len;

	struct ravl_interval_node *node;
	node = ravl_interval_find(rsv->itree, &map);
	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}

/*
 * vm_reservation_map_find_closest_prior -- find closest mapping neighbor
 *                                          prior to the current one
 */
struct pmem2_map *
vm_reservation_map_find_closest_prior(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len)
{
	struct pmem2_map map;
	map.addr = (char *)rsv->addr + reserv_offset;
	map.content_length = len;

	struct ravl_interval_node *node;
	node = ravl_interval_find_closest_prior(rsv->itree, &map);
	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}

/*
 * vm_reservation_map_find_closest_prior -- find closest mapping neighbor
 *                                          prior to the current one
 */
struct pmem2_map *
vm_reservation_map_find_closest_later(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len)
{
	struct pmem2_map map;
	map.addr = (char *)rsv->addr + reserv_offset;
	map.content_length = len;

	struct ravl_interval_node *node;
	node = ravl_interval_find_closest_later(rsv->itree, &map);
	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}
