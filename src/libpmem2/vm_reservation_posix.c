// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2021, Intel Corporation */

/*
 * vm_reservation_posix.c -- implementation of virtual memory
 *                           reservation API (POSIX)
 */

#include <sys/mman.h>

#include "alloc.h"
#include "map.h"
#include "mmap.h"
#include "out.h"
#include "pmem2_utils.h"

int vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize, bool align_addr);
int vm_reservation_release_memory(void *addr, size_t size);

/*
 * vm_reservation_reserve_memory -- (internal) create a blank virtual memory
 *                                             mapping, not backed by a file
 *
 * Address is an optional parameter.
 * Address and size should be a multiple of page size.
 *
 * Can be used to align randomly received address to the predicted alignment.
 * This feature doesn't work when a non-null address is provided.
 */
int
vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize, bool align_addr)
{
	size_t mmap_size = size;
	size_t align = Mmap_align;

	int map_flag = 0;
	if (addr) {
/*
 * glibc started exposing MAP_FIXED_NOREPLACE flag in version 4.17,
 * but even if the flag is not supported, we can imitate its behavior
 */
#ifdef MAP_FIXED_NOREPLACE
		map_flag = MAP_FIXED_NOREPLACE;
#endif
	} else if (align_addr) {
		align = util_map_hint_align(size, 0);
		mmap_size += align;
	}

	/*
	 * If the MAP_FIXED_NOREPLACE flag is supported and requested region is
	 * occupied, mmap will fail with EEXIST.
	 */
	char *mmap_addr = mmap(addr, mmap_size, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS | map_flag, -1, 0);
	if (mmap_addr == MAP_FAILED) {
		if (errno == EEXIST) {
			ERR("!mmap MAP_FIXED_NOREPLACE");
			return PMEM2_E_MAPPING_EXISTS;
		}
		ERR("!mmap MAP_ANONYMOUS");
		return PMEM2_E_ERRNO;
	}

	/*
	 * If kernel does not support the MAP_FIXED_NOREPLACE flag and provided
	 * address is occupied, kernel chooses new random address. To avoid this
	 * behavior, we validate requested and returned addresses.
	 * When requested address is not specified, any returned address
	 * is acceptable.
	 */
	if (addr && mmap_addr != addr) {
		munmap(mmap_addr, mmap_size);
		ERR("mapping exists in the given address");
		return PMEM2_E_MAPPING_EXISTS;
	}

	/* mmap size was enlarged for address alignment */
	if (mmap_size != size) {
		void *aligned_addr = (void *)ALIGN_UP((size_t)mmap_addr, align);
		size_t addr_diff = (size_t)aligned_addr - (size_t)mmap_addr;
		size_t size_diff = mmap_size - size;

		if (addr_diff) {
			if (munmap(mmap_addr, addr_diff)) {
				ERR("!munmap");
				return PMEM2_E_ERRNO;
			}
			size_diff -= addr_diff;
		}

		if (size_diff) {
			if (munmap(ADDR_SUM(aligned_addr, size), size_diff)) {
				ERR("!munmap");
				return PMEM2_E_ERRNO;
			}
		}

		mmap_addr = aligned_addr;
	}

	*raddr = mmap_addr;
	*rsize = size;

	return 0;
}

/*
 * vm_reservation_release_memory -- (internal) releases blank virtual memory
 *                                             mapping
 */
int
vm_reservation_release_memory(void *addr, size_t size)
{
	if (munmap(addr, size)) {
		ERR("!munmap");
		return PMEM2_E_ERRNO;
	}

	return 0;
}

/*
 * vm_reservation_extend_memory -- (internal) extend virtual memory mapping
 */
int
vm_reservation_extend_memory(struct pmem2_vm_reservation *rsv,
		void *rsv_end_addr, size_t size)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(rsv);

	void *reserved_addr = NULL;
	size_t reserved_size = 0;

	int ret = vm_reservation_reserve_memory(rsv_end_addr, size,
			&reserved_addr, &reserved_size, 0);
	if (ret)
		return ret;

	ASSERTeq(rsv_end_addr, reserved_addr);
	ASSERTeq(size, reserved_size);

	return ret;
}

/*
 * vm_reservation_shrink_memory -- (internal) shrink virtual memory mapping
 */
int
vm_reservation_shrink_memory(struct pmem2_vm_reservation *rsv,
		void *rsv_release_addr, size_t size)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(rsv);

	return vm_reservation_release_memory(rsv_release_addr, size);
}
