// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */

/*
 * pmem_posix.c -- pmem utilities with Posix implementation xD
 */

#include <stddef.h>
#include <sys/mman.h>

#include "pmem.h"
#include "out.h"
#include "mmap.h"

/*
 * is_pmem_detect -- implement pmem_is_pmem()
 *
 * This function returns true only if the entire range can be confirmed
 * as being direct access persistent memory.  Finding any part of the
 * range is not direct access, or failing to look up the information
 * because it is unmapped or because any sort of error happens, just
 * results in returning false.
 */
int
is_pmem_detect(const void *addr, size_t len)
{
	LOG(3, "addr %p len %zu", addr, len);

	if (len == 0)
		return 0;

	int retval = util_range_is_pmem(addr, len);

	LOG(4, "returning %d", retval);
	return retval;
}

/*
 * pmem_map_register -- memory map file and register mapping
 */
void *
pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax)
{
	LOG(3, "fd %d len %zu path %s id_dev_dax %d",
			fd, len, path, is_dev_dax);

	void *addr;
	int map_sync;
	addr = util_map(fd, 0, len, MAP_SHARED, 0, 0, &map_sync);
	if (!addr)
		return NULL;

	enum pmem_map_type type = MAX_PMEM_TYPE;
	if (is_dev_dax)
		type = PMEM_DEV_DAX;
	else if (map_sync)
		type = PMEM_MAP_SYNC;

	if (type != MAX_PMEM_TYPE) {
		if (util_range_register(addr, len, path, type)) {
			LOG(1, "can't track mapped region");
			goto err_unmap;
		}
	}

	return addr;
err_unmap:
	util_unmap(addr, len);
	return NULL;
}

/*
 * pmem_os_init -- os-dependent part of pmem initialization
 */
void
pmem_os_init(is_pmem_func *func)
{
	LOG(3, NULL);

	*func = is_pmem_detect;
}
