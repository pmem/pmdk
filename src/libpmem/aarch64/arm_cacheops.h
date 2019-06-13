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
 * ARM inline assembly to flush and invalidate caches
 * clwb => dc cvac
 * clflush | clflushopt => dc civac
 * fence => dmb ish
 */

/*
 * Cache instructions on ARM:
 * ARMv8.0-a    DC CVAC  - cache clean to Point of Coherency
 *                         Meant for thread synchronization, usually implies
 *                         real memory flush but may mean less.
 * ARMv8.2-a    DC CVAP  - cache clean to Point of Persistency
 *                         Meant exactly for our use.
 * ARMv8.5-a    DC CVADP - cache clean to Point of Deep Persistency
 *                         As of mid-2019 not on any commercially available CPU.
 * Any of the above may be disabled for EL0, but it's probably safe to consider
 * that a system configuration error.
 * Other flags include I (like "DC CIVAC") that invalidates the cache line, but
 * we don't want that.
 *
 * Memory fences:
 * * DMB [ISH]    MFENCE
 * * DMB [ISH]ST  SFENCE
 * * DMB [ISH]LD  LFENCE
 * We care about persistence not synchronization thus ISH should be enough?
 *
 * Memory domains:
 * * non-shareable - local to a single core
 * * inner shareable (ISH) - usu. one or multiple processor sockets
 * * outer shareable (OSH) - usu. including GPU
 * * full system (SY) - anything that can possibly access memory
 * ??? What about RDMA?  No libfabric on ARM thus not a concern for now.
 *
 * Exception (privilege) levels:
 * * EL0 - userspace (ring 3)
 * * EL1 - kernel (ring 0)
 * * EL2 - hypervisor (ring -1)
 * * EL3 - "secure world" (ring -3)
 */

#ifndef AARCH64_CACHEOPS_H
#define AARCH64_CACHEOPS_H

#include <stdlib.h>

static inline void
arm_clean_va_to_poc(void const *p __attribute__((unused)))
{
	asm volatile("dc cvac, %0" : : "r" (p) : "memory");
}

static inline void
arm_store_memory_barrier(void)
{
	asm volatile("dmb ishst" : : : "memory");
}

static inline void
arm_clean_and_invalidate_va_to_poc(const void *addr)
{
	asm volatile("dc civac, %0" : : "r" (addr) : "memory");
}
#endif
