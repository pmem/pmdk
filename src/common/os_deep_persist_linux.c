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
 * os_deep_persist_linux.c -- Linux abstraction layer
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "out.h"
#include "os.h"
#include "mmap.h"
#include "libpmem.h"
#include "os_deep_persist.h"
#include "common_deep_persist.h"

/*
 * os_range_deep_persist -- (internal) perform deep persist of
 * given address range
 */
int
os_range_deep_persist(uintptr_t addr, size_t len)
{
	LOG(3, "addr 0x%016" PRIxPTR " len %zu", addr, len);

	while (len != 0) {
		const struct map_tracker *mt = util_range_find(addr, len);

		if (mt == NULL) /* no more overlapping track regions */
			return pmem_msync((void *)addr, len);
/*
 * for input deep_persist range that cover found mapping
 * it call write to deep_flush file, for addresses away
 * from tracker range it calls msync
 */
		if (mt->base_addr > addr) {
			size_t curr_len = mt->base_addr - addr;
			if (curr_len > len)
				curr_len = len;
			if (pmem_msync((void *)addr, curr_len) != 0)
				return -1;
			if ((len -= curr_len) == 0)
				return 0;
			addr = mt->base_addr;
		}
		size_t mt_len = mt->end_addr - mt->base_addr;
		pmem_persist((void *)mt->base_addr, mt_len);
		if (ddax_deep_flush_write(mt->region_id) < 0) {
			ERR("cannot write to deep_flush in region %d",
				mt->region_id);
			return -1;
		}

		if (mt->end_addr >= addr + len)
			return 0;

		size_t diff = mt->end_addr - addr;
		len -= diff;
		addr = mt->end_addr;
	}
	return 0;
}
