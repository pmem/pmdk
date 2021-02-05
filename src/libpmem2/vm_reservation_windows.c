// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * vm_reservation_windows.c -- implementation of virtual memory
 *                             reservation API (Windows)
 */

#include "alloc.h"
#include "map.h"
#include "os_thread.h"
#include "out.h"
#include "pmem2_utils.h"
#include "sys_util.h"

int vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize);
int vm_reservation_release_memory(void *addr, size_t size);
struct pmem2_map *vm_reservation_map_find_closest_prior(
		struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len);
struct pmem2_map *vm_reservation_map_find_closest_later(
		struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len);
struct ravl_interval *vm_reservation_get_interval_tree(
		struct pmem2_vm_reservation *rsv);

/*
 * vm_reservation_reserve_memory -- create a blank virtual memory mapping
 */
int
vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize)
{
	void *daddr = VirtualAlloc2(GetCurrentProcess(),
		addr,
		size,
		MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
		PAGE_NOACCESS,
		NULL,
		0);

	if (daddr == NULL) {
		ERR("!!VirtualAlloc2");
		DWORD ret_windows = GetLastError();
		if (ret_windows == ERROR_INVALID_ADDRESS)
			return PMEM2_E_MAPPING_EXISTS;
		else
			return pmem2_lasterror_to_err();
	}

	*raddr = daddr;
	*rsize = size;

	return 0;
}

/*
 * vm_reservation_release_memory -- releases blank virtual memory mapping
 */
int
vm_reservation_release_memory(void *addr, size_t size)
{
	int ret = VirtualFree(addr,
		0,
		MEM_RELEASE);
	if (!ret) {
		ERR("!!VirtualFree");
		return pmem2_lasterror_to_err();
	}

	return 0;
}

/*
 * vm_reservation_map_find_closest_prior -- find closest mapping neighbor
 *                                          prior to the provided mapping
 */
struct pmem2_map *
vm_reservation_map_find_closest_prior(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len)
{
	struct pmem2_map map;

	map.addr = (char *)pmem2_vm_reservation_get_address(rsv) +
			reserv_offset;
	map.content_length = len;

	struct ravl_interval_node *node;
	struct ravl_interval *itree = vm_reservation_get_interval_tree(rsv);
	node = ravl_interval_find_closest_prior(itree, &map);

	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}

/*
 * vm_reservation_map_find_closest_later -- find closest mapping neighbor later
 *                                          than the mapping provided
 */
struct pmem2_map *
vm_reservation_map_find_closest_later(struct pmem2_vm_reservation *rsv,
		size_t reserv_offset, size_t len)
{
	struct pmem2_map map;
	map.addr = (char *)pmem2_vm_reservation_get_address(rsv) +
			reserv_offset;
	map.content_length = len;

	struct ravl_interval_node *node;
	struct ravl_interval *itree = vm_reservation_get_interval_tree(rsv);
	node = ravl_interval_find_closest_later(itree, &map);

	if (!node)
		return NULL;

	return (struct pmem2_map *)ravl_interval_data(node);
}

/*
 * vm_reservation_merge_placeholders -- merge the placeholder reservations
 *                                      into one bigger placeholder
 */
int
vm_reservation_merge_placeholders(struct pmem2_vm_reservation *rsv, void *addr,
		size_t length)
{
	LOG(3, "rsv %p addr %p length %zu", rsv, addr, length);

	void *rsv_addr = pmem2_vm_reservation_get_address(rsv);
	size_t rsv_size = pmem2_vm_reservation_get_size(rsv);
	size_t rsv_offset = (size_t)addr - (size_t)rsv_addr;

	LOG(3, "rsv_addr %p rsv_size %zu", rsv_addr, rsv_size);

	/*
	 * After unmapping from the reservation, it is neccessary to merge
	 * the unoccupied neighbours so that the placeholders will be available
	 * for splitting for the required size of the mapping.
	 */
	void *merge_addr = addr;
	size_t merge_size = length;
	struct pmem2_map *map = NULL;

	if (rsv_offset > 0) {
		map = vm_reservation_map_find_closest_prior(rsv, rsv_offset,
				length);
		if (map) {
			/*
			 * Find the closest mapping prior to the provided range
			 * (addr, addr + length), if any mapping was found then
			 * then set the merge address as its ending address.
			 * Increase the merge size by the size between the new
			 * merge address and the initial merge address.
			 */
			merge_addr = (char *)map->addr + map->reserved_length;
			merge_size += (char *)addr - (char *)merge_addr;
		} else {
			/*
			 * If no mapping was found then set the merge address
			 * at the start of the reservation and increase the
			 * merge size by the offset into the reservation that
			 * the initial region for the merging was situated at.
			 */
			merge_addr = rsv_addr;
			merge_size += rsv_offset;
		}
	}

	if (rsv_offset + length < rsv_size) {
		map = vm_reservation_map_find_closest_later(rsv, rsv_offset,
				length);
		if (map) {
			/*
			 * Find the closest mapping after the provided range
			 * (addr, addr + length), if any mapping was found then
			 * increase the merge size by the size between initial
			 * end address (addr + size) and the starting address
			 * of the found mapping.
			 */
			merge_size += (char *)map->addr - (char *)addr - length;
		} else {
			/*
			 * If no mapping was found then set the increase the
			 * merge size by the size between initial end address
			 * (addr + size) and the end of the reservation.
			 */
			merge_size += rsv_size - rsv_offset - length;
		}
	}

	if ((addr != merge_addr) || (length != merge_size)) {
		int ret = VirtualFree(merge_addr,
			merge_size,
			MEM_RELEASE | MEM_COALESCE_PLACEHOLDERS);
		if (!ret) {
			ERR("!!VirtualFree");
			return pmem2_lasterror_to_err();

		}
	}

	return 0;
}

/*
 * vm_reservation_split_placeholders - splits the virtual memory reservation
 *                                     into separate placeholders
 */
int
vm_reservation_split_placeholders(struct pmem2_vm_reservation *rsv, void *addr,
		size_t length)
{
	LOG(3, "rsv %p addr %p length %zu", rsv, addr, length);

	void *rsv_addr = pmem2_vm_reservation_get_address(rsv);
	size_t rsv_size = pmem2_vm_reservation_get_size(rsv);
	size_t rsv_offset = (size_t)addr - (size_t)rsv_addr;

	LOG(3, "rsv_addr %p rsv_size %zu", rsv_addr, rsv_size);

	/*
	 * Check wheter there is a mapping just beside the provided placeholder
	 * range (addr, addr + length). If the immediate neighboring ranges
	 * are empty then split the provided range into separate placeholder.
	 */
	struct pmem2_map *any_map;
	if ((rsv_offset > 0 && pmem2_vm_reservation_map_find(rsv,
			rsv_offset - 1, 1, &any_map)) ||
			(rsv_offset + length < rsv_size &&
			pmem2_vm_reservation_map_find(rsv,
			rsv_offset + length, 1, &any_map))) {
		/* split the placeholder */
		int ret = VirtualFree((char *)rsv_addr + rsv_offset,
			length,
			MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
		if (!ret) {
			ERR("!!VirtualFree");
			ret = pmem2_lasterror_to_err();
			return ret;
		}
	}

	return 0;
}

/*
 * vm_reservation_extend_memory -- extend memory range the reservation covers
 */
int
vm_reservation_extend_memory(struct pmem2_vm_reservation *rsv,
		void *rsv_end_addr, size_t size)
{
	void *reserved_addr = NULL;
	size_t reserved_size = 0;

	int ret = vm_reservation_reserve_memory(rsv_end_addr, size,
			&reserved_addr, &reserved_size);
	if (ret)
		return ret;

	ASSERTeq(rsv_end_addr, reserved_addr);
	ASSERTeq(size, reserved_size);

	ret = vm_reservation_merge_placeholders(rsv, rsv_end_addr, size);
	if (ret)
		vm_reservation_release_memory(rsv_end_addr, size);

	return ret;
}

/*
 * vm_reservation_shrink_memory -- shrink memory range the reservation covers
 */
int
vm_reservation_shrink_memory(struct pmem2_vm_reservation *rsv,
		void *rsv_release_addr, size_t size)
{
	int ret = vm_reservation_split_placeholders(rsv, rsv_release_addr,
			size);
	if (ret)
		return ret;

	ret = vm_reservation_release_memory(rsv_release_addr, size);
	if (ret)
		vm_reservation_merge_placeholders(rsv, rsv_release_addr, size);

	return ret;
}
