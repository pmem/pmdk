// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */
/*
 * Copyright (c) 2015-2017, Microsoft Corporation. All rights reserved.
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
 * mmap_windows.c -- memory-mapped files for Windows
 */

#include <sys/mman.h>
#include "mmap.h"
#include "out.h"

/*
 * util_map_hint_unused -- use VirtualQuery to determine hint address
 *
 * This is a helper function for util_map_hint().
 * It iterates through memory regions and looks for the first unused address
 * in the process address space that is:
 * - greater or equal 'minaddr' argument,
 * - large enough to hold range of given length,
 * - aligned to the specified unit.
 */
char *
util_map_hint_unused(void *minaddr, size_t len, size_t align)
{
	LOG(3, "minaddr %p len %zu align %zu", minaddr, len, align);

	ASSERT(align > 0);

	MEMORY_BASIC_INFORMATION mi;
	char *lo = NULL;	/* beginning of current range in maps file */
	char *hi = NULL;	/* end of current range in maps file */
	char *raddr = minaddr;	/* ignore regions below 'minaddr' */

	if (raddr == NULL)
		raddr += Pagesize;

	raddr = (char *)roundup((uintptr_t)raddr, align);

	while ((uintptr_t)raddr < UINTPTR_MAX - len) {
		size_t ret = VirtualQuery(raddr, &mi, sizeof(mi));
		if (ret == 0) {
			ERR("VirtualQuery %p", raddr);
			return MAP_FAILED;
		}
		LOG(4, "addr %p len %zu state %d",
			mi.BaseAddress, mi.RegionSize, mi.State);

		if ((mi.State != MEM_FREE) || (mi.RegionSize < len)) {
			raddr = (char *)mi.BaseAddress + mi.RegionSize;
			raddr = (char *)roundup((uintptr_t)raddr, align);
			LOG(4, "nearest aligned addr %p", raddr);
		} else {
			LOG(4, "unused region of size %zu found at %p",
				mi.RegionSize, mi.BaseAddress);
			return mi.BaseAddress;
		}
	}

	LOG(4, "end of address space reached");
	return MAP_FAILED;
}

/*
 * util_map_hint -- determine hint address for mmap()
 *
 * XXX - Windows doesn't support large DAX pages yet, so there is
 * no point in aligning for the same.
 */
char *
util_map_hint(size_t len, size_t req_align)
{
	LOG(3, "len %zu req_align %zu", len, req_align);

	char *hint_addr = MAP_FAILED;

	/* choose the desired alignment based on the requested length */
	size_t align = util_map_hint_align(len, req_align);

	if (Mmap_no_random) {
		LOG(4, "user-defined hint %p", Mmap_hint);
		hint_addr = util_map_hint_unused(Mmap_hint, len, align);
	} else {
		/*
		 * Create dummy mapping to find an unused region of given size.
		 * Request for increased size for later address alignment.
		 *
		 * Use MAP_NORESERVE flag to only reserve the range of pages
		 * rather than commit.  We don't want the pages to be actually
		 * backed by the operating system paging file, as the swap
		 * file is usually too small to handle terabyte pools.
		 */
		char *addr = mmap(NULL, len + align, PROT_READ,
				MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
		if (addr != MAP_FAILED) {
			LOG(4, "system choice %p", addr);
			hint_addr = (char *)roundup((uintptr_t)addr, align);
			munmap(addr, len + align);
		}
	}

	LOG(4, "hint %p", hint_addr);
	return hint_addr;
}

/*
 * util_map_sync -- memory map given file into memory
 */
void *
util_map_sync(void *addr, size_t len, int proto, int flags, int fd,
	os_off_t offset, int *map_sync)
{
	LOG(15, "addr %p len %zu proto %x flags %x fd %d offset %ld",
		addr, len, proto, flags, fd, offset);

	if (map_sync)
		*map_sync = 0;

	return mmap(addr, len, proto, flags, fd, offset);
}
