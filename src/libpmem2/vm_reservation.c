// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * vm_reservation.c -- implementation of virtual memory allocation API
 */

#include "alloc.h"
#include "map.h"
#include "pmem2_utils.h"
#include "ravl_interval.h"
#include "sys_util.h"
#include "vm_reservation.h"

#ifdef _WIN32
#include <Windows.h>
#endif

struct pmem2_vm_reservation {
	struct ravl_interval *itree;
	void *addr;
	size_t size;
	os_rwlock_t lock;
};

int vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize);
int vm_reservation_release_memory(void *addr, size_t size);
struct ravl_interval *vm_reservation_get_interval_tree(
		struct pmem2_vm_reservation *rsv);

/*
 * pmem2_vm_reservation_get_address -- get reservation address
 */
void *
pmem2_vm_reservation_get_address(struct pmem2_vm_reservation *rsv)
{
	LOG(3, "reservation %p", rsv);
	/* we do not need to clear err because this function cannot fail */

	return rsv->addr;
}

/*
 * pmem2_vm_reservation_get_size -- get reservation size
 */
size_t
pmem2_vm_reservation_get_size(struct pmem2_vm_reservation *rsv)
{
	LOG(3, "reservation %p", rsv);
	/* we do not need to clear err because this function cannot fail */

	return rsv->size;
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

/*
 * pmem2_vm_reservation_init - initialize the reservation structure
 */
static int
vm_reservation_init(struct pmem2_vm_reservation *rsv)
{
	util_rwlock_init(&rsv->lock);

	rsv->itree = ravl_interval_new(mapping_min, mapping_max);
	if (!rsv->itree)
		return -1;

	return 0;
}

/*
 * pmem2_vm_reservation_fini - finalize the reservation structure
 */
static void
vm_reservation_fini(struct pmem2_vm_reservation *rsv)
{
	ravl_interval_delete(rsv->itree);
	util_rwlock_destroy(&rsv->lock);
}

/*
 * pmem2_vm_reservation_new -- creates new virtual memory reservation
 */
int
pmem2_vm_reservation_new(struct pmem2_vm_reservation **rsv_ptr,
	void *addr, size_t size)
{
	PMEM2_ERR_CLR();
	*rsv_ptr = NULL;

	/*
	 * base address has to be aligned to the allocation granularity
	 * on Windows, and to the page size otherwise
	 */
	if (addr && (unsigned long long)addr % Mmap_align) {
		ERR("address %p is not a multiple of 0x%llx", addr,
			Mmap_align);
		return PMEM2_E_ADDRESS_UNALIGNED;
	}

	/*
	 * size should be aligned to the allocation granularity on Windows,
	 * and to the page size otherwise
	 */
	if (size % Mmap_align) {
		ERR("reservation size %zu is not a multiple of %llu",
			size, Mmap_align);
		return PMEM2_E_LENGTH_UNALIGNED;
	}

	int ret;
	struct pmem2_vm_reservation *rsv = pmem2_malloc(
			sizeof(struct pmem2_vm_reservation), &ret);
	if (ret)
		return ret;

	/* initialize the ravl interval tree */
	ret = vm_reservation_init(rsv);
	if (ret)
		goto err_rsv_init;

	void *raddr = NULL;
	size_t rsize = 0;
	ret = vm_reservation_reserve_memory(addr, size, &raddr, &rsize);
	if (ret)
		goto err_reserve;

	rsv->addr = raddr;
	rsv->size = rsize;

	*rsv_ptr = rsv;

	return 0;

err_reserve:
	vm_reservation_fini(rsv);
err_rsv_init:
	Free(rsv);
	return ret;
}

/*
 * pmem2_vm_reservation_delete -- deletes reservation bound to
 *                                the pmem2_vm_reservation structure
 */
int
pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv_ptr)
{
	PMEM2_ERR_CLR();

	struct pmem2_vm_reservation *rsv = *rsv_ptr;

	struct pmem2_map *any_map;
	/* check if reservation contains any mapping */
	if (!pmem2_vm_reservation_map_find(rsv, 0, rsv->size, &any_map)) {
		ERR("vm reservation %p isn't empty", rsv);
		return PMEM2_E_VM_RESERVATION_NOT_EMPTY;
	}

	int ret = vm_reservation_release_memory(rsv->addr, rsv->size);
	if (ret)
		return ret;

	vm_reservation_fini(rsv);
	Free(rsv);

	return 0;
}

/*
 * pmem2_vm_reservation_map_find -- find the earliest mapping overlapping
 *                                  with (addr, addr+size) range
 */
int
pmem2_vm_reservation_map_find(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len, struct pmem2_map **map)
{
	PMEM2_ERR_CLR();
	LOG(3, "reservation %p reserv_offset %zu length %zu pmem2_map %p",
			rsv, reserv_offset, len, map);

	*map = NULL;

	struct pmem2_map dummy_map;
	dummy_map.addr = (char *)rsv->addr + reserv_offset;
	dummy_map.content_length = len;

	struct ravl_interval_node *node;
	node = ravl_interval_find(rsv->itree, &dummy_map);
	if (!node)
		return PMEM2_E_MAPPING_NOT_FOUND;

	*map = (struct pmem2_map *)ravl_interval_data(node);

	return 0;
}

/*
 * vm_reservation_map_register_release -- register mapping in the mappings tree
 * of reservation structure and release previously acquired lock regardless
 * of the success or failure of the function.
 */
int
vm_reservation_map_register_release(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map)
{
	int ret = ravl_interval_insert(rsv->itree, map);
	if (ret == -EEXIST) {
		ERR(
			"mapping at the given region of the reservation already exist");
		ret = PMEM2_E_MAPPING_EXISTS;
	}

	util_rwlock_unlock(&rsv->lock);

	return ret;
}

/*
 * vm_reservation_map_unregister_release -- unregister mapping from the mapping
 * tree of reservation structure and release previously acquired lock regardless
 * of the success or failure of the function.
 */
int
vm_reservation_map_unregister_release(struct pmem2_vm_reservation *rsv,
		struct pmem2_map *map)
{
	int ret = 0;
	struct ravl_interval_node *node;

	node = ravl_interval_find_equal(rsv->itree, map);
	if (!(node && !ravl_interval_remove(rsv->itree, node))) {
		ERR("Cannot find mapping %p in the reservation %p",
				map, rsv);
		ret = PMEM2_E_MAPPING_NOT_FOUND;
	}

	util_rwlock_unlock(&rsv->lock);

	return ret;
}

/*
 * vm_reservation_map_find_acquire -- find the earliest mapping overlapping
 * with (addr, addr+size) range. This function acquires a lock and keeps it
 * until next release operation.
 */
struct pmem2_map *
vm_reservation_map_find_acquire(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len)
{
	struct pmem2_map map;
	map.addr = (char *)rsv->addr + reserv_offset;
	map.content_length = len;

	struct ravl_interval_node *node;

	util_rwlock_wrlock(&rsv->lock);
	node = ravl_interval_find(rsv->itree, &map);

	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}

/*
 * vm_reservation_release -- releases previously acquired lock
 */
void
vm_reservation_release(struct pmem2_vm_reservation *rsv)
{
	util_rwlock_unlock(&rsv->lock);
}

/*
 * vm_reservation_get_interval_tree -- get interval tree
 */
struct ravl_interval *
vm_reservation_get_interval_tree(struct pmem2_vm_reservation *rsv)
{
	return rsv->itree;
}

/*
 * pmem2_vm_reservation_extend -- extend the reservation from the end by the
 *                                given size, keeps the entries
 */
int
pmem2_vm_reservation_extend(struct pmem2_vm_reservation *rsv, size_t size)
{
	LOG(3, "reservation %p size %zu", rsv, size);
	PMEM2_ERR_CLR();

	void *rsv_end_addr = (char *)rsv->addr + rsv->size;

	if (size % Mmap_align) {
		ERR("reservation extension size %zu is not a multiple of %llu",
			size, Pagesize);
		return PMEM2_E_LENGTH_UNALIGNED;
	}

	util_rwlock_wrlock(&rsv->lock);
	rsv->size += size;
	int ret = vm_reservation_extend_memory(rsv, rsv_end_addr, size);
	if (ret)
		rsv->size -= size;
	util_rwlock_unlock(&rsv->lock);

	return ret;
}

/*
 * pmem2_vm_reservation_shrink -- reduce the reservation by the
 *                                interval (offset, size)
 */
int
pmem2_vm_reservation_shrink(struct pmem2_vm_reservation *rsv, size_t offset,
		size_t size)
{
	LOG(3, "reservation %p offset %zu size %zu", rsv, offset, size);
	PMEM2_ERR_CLR();

	if (offset % Mmap_align) {
		ERR("reservation shrink offset %zu is not a multiple of %llu",
			offset, Mmap_align);
		return PMEM2_E_OFFSET_UNALIGNED;
	}

	if (size % Mmap_align) {
		ERR("reservation shrink size %zu is not a multiple of %llu",
			size, Mmap_align);
		return PMEM2_E_LENGTH_UNALIGNED;
	}

	if (offset >= rsv->size) {
		ERR("reservation shrink offset %zu is out of reservation range",
			offset);
		return PMEM2_E_OFFSET_OUT_OF_RANGE;
	}

	if (size == 0) {
		ERR("reservation shrink size %zu cannot be zero",
			size);
		return PMEM2_E_LENGTH_OUT_OF_RANGE;
	}

	if ((offset + size) > rsv->size) {
		ERR(
			"reservation shrink size %zu stands out of reservation range",
			size);
		return PMEM2_E_LENGTH_OUT_OF_RANGE;
	}

	if (offset != 0 && (offset + size) != rsv->size) {
		ERR("shrinking reservation from the middle is not supported");
		return PMEM2_E_NOSUPP;
	}

	if (offset == 0 && size == rsv->size) {
		ERR("shrinking whole reservation is not supported");
		return PMEM2_E_NOSUPP;
	}

	struct pmem2_map *any_map;
	if (!pmem2_vm_reservation_map_find(rsv, offset, size, &any_map)) {
		ERR(
			"reservation region (offset %zu, size %zu) to be shrunk is occupied by a mapping",
			offset, size);
		return PMEM2_E_VM_RESERVATION_NOT_EMPTY;
	}

	/* XXX: try unmapping without splitting the placeholder */
	void *rsv_release_addr = (char *)rsv->addr + offset;

	util_rwlock_wrlock(&rsv->lock);
	int ret = vm_reservation_shrink_memory(rsv, rsv_release_addr, size);
	if (offset == 0)
		rsv->addr = (char *)rsv->addr + size;
	rsv->size -= size;
	util_rwlock_unlock(&rsv->lock);

	return ret;
}
