// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020-2024, Intel Corporation */

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
		size_t *rsize);
int vm_reservation_release_memory(void *addr, size_t size);

/*
 * vm_reservation_get_map_alignment -- (internal) choose the desired mapping
 *                                                alignment
 *
 * This function tries to default to the largest possible alignment (page size),
 * unless forbidden by the underlying memory source.
 *
 * Use 1GB page alignment only if the mapping length is at least
 * twice as big as the page size.
 */
size_t
vm_reservation_get_map_alignment(size_t len, size_t min_align)
{
	size_t align = 2 * MEGABYTE;
	if (len >= 2 * GIGABYTE)
		align = GIGABYTE;

	if (align < min_align)
		align = min_align;

	return align;
}

/*
 * vm_reservation_reserve_memory -- (internal) create a blank virtual memory
 * mapping, not backed by a file.
 *
 * Address is an optional parameter.
 * Address and size should be a multiple of page size.
 *
 * If address is NULL, the address will be chosen randomly by the OS.
 */
int
vm_reservation_reserve_memory(void *addr, size_t size, void **raddr,
		size_t *rsize)
{
	size_t mmap_size = size;

	int mmap_flag = 0;
	if (addr) {
/*
 * glibc started exposing MAP_FIXED_NOREPLACE flag in version 4.17,
 * but even if the flag is not supported, we can imitate its behavior
 */
#ifdef MAP_FIXED_NOREPLACE
		mmap_flag = MAP_FIXED_NOREPLACE;
#endif
	}

	/*
	 * If the MAP_FIXED_NOREPLACE flag is supported and requested region is
	 * occupied, mmap will fail with EEXIST.
	 */
	void *mmap_addr = mmap(addr, mmap_size, PROT_NONE,
			MAP_PRIVATE | MAP_ANONYMOUS | mmap_flag, -1, 0);
	if (mmap_addr == MAP_FAILED) {
		if (errno == EEXIST) {
			ERR_W_ERRNO("mmap MAP_FIXED_NOREPLACE");
			return PMEM2_E_MAPPING_EXISTS;
		}
		ERR_W_ERRNO("mmap MAP_ANONYMOUS");
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
		ERR_WO_ERRNO("mapping exists in the given address");
		return PMEM2_E_MAPPING_EXISTS;
	}

	*raddr = mmap_addr;
	*rsize = mmap_size;

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
		ERR_W_ERRNO("munmap");
		return PMEM2_E_ERRNO;
	}

	return 0;
}

/*
 * vm_reservation_extend_memory -- (internal) extend virtual memory mapping
 */
int
vm_reservation_extend_memory(struct pmem2_vm_reservation *rsv, void *addr,
		size_t size)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(rsv);

	void *reserved_addr = NULL;
	size_t reserved_size = 0;

	int ret = vm_reservation_reserve_memory(addr, size,
			&reserved_addr, &reserved_size);
	if (ret)
		return ret;

	ASSERTeq(reserved_addr, addr);
	ASSERTeq(reserved_size, size);

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
