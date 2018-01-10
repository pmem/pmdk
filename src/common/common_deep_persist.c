/*
 * Copyright 2017-2018, Intel Corporation
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
 * common_deep_persist.c -- internal utility functions for flushing
 * a memory range.
 * Currently only used on Linux.
 */

#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "os.h"
#include "file.h"
#include "out.h"
#include "common_deep_persist.h"
#include "os_deep_persist.h"
#include "libpmem.h"

/*
 * ddax_deep_flush_write -- perform final write to deep_flush on given region_id
 */
int
ddax_deep_flush_write(int region_id)
{
	LOG(3, "region_id %d", region_id);

	char deep_flush_path[PATH_MAX];
	int deep_flush_fd;

	snprintf(deep_flush_path, PATH_MAX,
		"/sys/bus/nd/devices/region%d/deep_flush", region_id);

	if ((deep_flush_fd = os_open(deep_flush_path, O_RDWR)) < 0) {
		ERR("!os_open(\"%s\", O_RDWR)", deep_flush_path);
		return -1;
	}

	if (write(deep_flush_fd, "1", 1) != 1) {
		ERR("!write(%d, \"1\")", deep_flush_fd);
		int oerrno = errno;
		os_close(deep_flush_fd);
		errno = oerrno;
		return -1;
	}

	os_close(deep_flush_fd);
	return 0;
}

/*
 * common_replica_deep_persist -- perform deep persist on replica's parts
 * from range for dev dax write to deep_flush, otherwise run msync
 */
static int
common_replica_deep_persist(const void *addr, size_t len,
			struct pool_set *set, unsigned replica_id)
{
	LOG(3, "addr %p len %zu set %p replica_id %u",
		addr, len, set, replica_id);

	struct pool_replica *rep = set->replica[replica_id];
	uintptr_t rep_addr = (uintptr_t)rep->part[0].addr;
	uintptr_t rep_end = rep_addr + rep->repsize;
	uintptr_t end = (uintptr_t)addr + len;

	ASSERT((uintptr_t)addr >= rep_addr);
	ASSERT(end <= rep_end);

	int region_id = -1;
	int old_id = -1;
	for (unsigned p = 0; p < rep->nparts; p++) {
		struct pool_set_part *part = &rep->part[p];
		uintptr_t padd = (uintptr_t)part->addr;
		uintptr_t pend = padd + part->size;
		uintptr_t isa = (uintptr_t)addr;
		uintptr_t ise = end;
		if (padd <= end && pend >= (uintptr_t)addr) {
			if (padd > (uintptr_t)addr)
				isa = padd;
			if (pend < end)
				ise = pend;
			size_t islen = ise - isa;
			if (part->is_dev_dax) {
				region_id = util_region_find(part->path);
				ASSERTne(region_id, -1);
				if (old_id == region_id)
					continue;
				old_id = region_id;
				pmem_persist((void *)isa, islen);
				if (ddax_deep_flush_write(region_id)) {
					ERR("ddax_deep_flush_write(%d)",
						region_id);
					return -1;
				}
			} else
				pmem_msync((void *)isa, islen);
		}
	}
	return 0;
}

/*
 * common_deep_persist - check if deep persist request is for pmem or other
 * and then perform proper deep_persist
 */
int
common_deep_persist(const void *addr, size_t len, struct pool_set *set,
		unsigned replica_id)
{
	LOG(3, "%p len %zu set %p replica_id %u",
		addr, len, set, replica_id);

/*
 * for internal deep_persist usage functions pass
 * pool_set to common_deep_persist that allow match
 * pool_set parts to DAX region_id, external pmem API
 * uses map_tracker to track DAX mappings and does
 * not use pool_sets, so it that case set should be NULL
 */
	if (set == NULL) {
		if (pmem_deep_persist(addr, len)) {
			ERR("!pmem_deep_persist(%p, %lu)", addr, len);
			return -1;
		}
	} else {
		if (common_replica_deep_persist(addr, len, set, replica_id)) {
			ERR("!common_replica_deep_persist(%p, %lu, %p, %u)",
				addr, len, set, replica_id);
			return -1;
		}
	}

	return 0;
}
