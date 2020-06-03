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
 * pmem2_vm_reservation_delete -- deletes reservation bound to
 *                                structure pmem2_vm_reservation
 */
int
pmem2_vm_reservation_delete(struct pmem2_vm_reservation **rsv_ptr)
{
	struct pmem2_vm_reservation *rsv = *rsv_ptr;

	/* Check if reservation contains any mapping */
	if (vm_reservation_map_find(rsv, 0, rsv->size))
		return PMEM2_E_VM_RESERVATION_NOT_EMPTY;

	int ret = VirtualFree(rsv->addr,
		0,
		MEM_RELEASE);
	if (!ret) {
		ERR("!!VirtualFree");
		return pmem2_lasterror_to_err();
	}

	vm_reservation_fini(rsv);
	Free(rsv);

	return 0;
}
