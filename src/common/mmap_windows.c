/*
 * Copyright 2015-2017, Intel Corporation
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
 * util_map_hint -- determine hint address for mmap()
 *
 * XXX - Windows doesn't support large DAX pages yet, so there is
 * no point in aligning for the same.
 */
char *
util_map_hint(int fd, size_t len, size_t req_align)
{
	LOG(3, "fd %d len %zu req_align %zu", fd, len, req_align);

	char *hint_addr = MAP_FAILED;

	/* choose the desired alignment based on the requested length */
	size_t align = util_map_hint_align(len, req_align);

	if (Mmap_no_random)
		LOG(4, "user-defined hint %p", (void *)Mmap_hint);

	/*
	 * Create dummy mapping to find an unused region of given size.
	 * Request for increased size for later address alignment.
	 *
	 * For large anonymous mappings, we can end up with error
	 * ERROR_COMMITMENT_LIMIT (0x5AF) - The paging file is too small
	 * for this operation to complete.  So, instead of anonymous mapping
	 * (as on Linux) we do the mapping on the actual file.
	 */
	char *addr = mmap(Mmap_hint, len + align, PROT_READ,
				MAP_PRIVATE, fd, 0);
	if (addr != MAP_FAILED) {
		LOG(4, "system choice %p", addr);
		hint_addr = (char *)roundup((uintptr_t)addr, align);
		munmap(addr, len + align);
	}
	LOG(4, "hint %p", hint_addr);

	return hint_addr;
}
