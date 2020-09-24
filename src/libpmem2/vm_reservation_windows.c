// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

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
 * vm_reservation_reserve_memory -- create a blank virual memory mapping
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
