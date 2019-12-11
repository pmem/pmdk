/*
 * Copyright 2019, IBM Corporation
 * Copyright 2019, Intel Corporation
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
#include "platform_generic.h"

#include "util.h"
#include "out.h"
#include "pmem2_arch.h"

static void
ppc_fence(void)
{
	LOG(15, NULL);

	/*
	 * Force a memory barrier to flush out all cache lines
	 */
	asm volatile(
		"lwsync"
		: : : "memory");
}

static void
ppc_flush(const void *addr, size_t size)
{
	LOG(15, "addr %p size %zu", addr, size);

	uintptr_t uptr = (uintptr_t)addr;
	uintptr_t end = uptr + size;

	/* round down the address */
	uptr &= ~(CACHELINE_SIZE - 1);
	while (uptr < end) {
		/* issue a dcbst instruction for the cache line */
		asm volatile(
			"dcbst 0,%0"
			: :"r"(uptr) : "memory");

		uptr += CACHELINE_SIZE;
	}
}

int
platform_init(struct pmem2_arch_info *info)
{
	LOG(3, "Initializing Platform");

	info->fence = ppc_fence;
	info->deep_flush = ppc_flush;

	return 0;
}
