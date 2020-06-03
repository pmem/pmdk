// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * vm_reservation_windows.c -- implementation of virtual memory
 *                             reservation API (Windows)
 */

#include "alloc.h"
#include "map.h"
#include "out.h"
#include "pmem2_utils.h"

int vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize);
int vm_reservation_release_memory(void *addr, size_t size);

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
