/*
 * Copyright 2014-2017, Intel Corporation
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
 * pmem_linux.c -- pmem utilities with OS-specific implementation
 */

#include <stddef.h>
#include <errno.h>
#include <pthread.h>

#include "pmem.h"
#include "out.h"
#include "mmap.h"
#include "sys_util.h"

/*
 * is_pmem_proc -- implement pmem_is_pmem()
 *
 * This function returns true only if the entire range can be confirmed
 * as being direct access persistent memory.  Finding any part of the
 * range is not direct access, or failing to look up the information
 * because it is unmapped or because any sort of error happens, just
 * results in returning false.
 */
int
is_pmem_proc(const void *addr, size_t len)
{
	LOG(10, "addr %p len %zu", addr, len);

	int retval = 1;

	if (pthread_rwlock_rdlock(&Mmap_list_lock)) {
		errno = EBUSY;
		ERR("!cannot lock map tracking list");
		return 0;
	}

	while (len > 0) {
		map_tracker *mt = util_range_find(addr, len);
		if (mt == NULL) {
			LOG(4, "address not found %p", addr);
			retval = 0;
			break;
		}

		LOG(10, "range found - begin %p end %p flags %x",
				mt->base_addr, mt->end_addr, mt->flags);

		if (mt->base_addr > addr) {
			LOG(10, "base address doesn't match: %p > %p",
					mt->base_addr, addr);
			retval = 0;
			break;
		}

		retval &= ((mt->flags & MTF_DIRECT_MAPPED) != 0);

		uintptr_t map_len = ((uintptr_t)mt->end_addr - (uintptr_t)addr);
		if (map_len > len)
			map_len = len;
		len -= map_len;
		addr = (char *)addr + map_len;
	}

	util_rwlock_unlock(&Mmap_list_lock);

	LOG(3, "returning %d", retval);
	return retval;
}
