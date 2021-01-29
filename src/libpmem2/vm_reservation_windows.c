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
static int
vm_reservation_merge_placeholders(struct pmem2_vm_reservation *rsv,
		void *addr, size_t length)
{
	void *rsv_addr = pmem2_vm_reservation_get_address(rsv);
	size_t rsv_size = pmem2_vm_reservation_get_size(rsv);
	size_t rsv_offset = (size_t)addr - (size_t)rsv_addr;

	void *merge_addr = addr;
	size_t merge_size = length;
	struct pmem2_map *map = NULL;

	if (rsv_offset > 0) {
		map = vm_reservation_map_find_closest_prior(rsv, rsv_offset,
			length);
		if (map) {
			merge_addr = (char *)map->addr + map->reserved_length;
			merge_size += (char *)addr - (char *)merge_addr;
		} else {
			merge_addr = rsv_addr;
			merge_size += rsv_offset;
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
