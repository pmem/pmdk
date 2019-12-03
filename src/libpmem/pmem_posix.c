/*
 * Copyright 2014-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmem_posix.c -- pmem utilities with Posix implementation
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
