// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2019, Intel Corporation */
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
#endif
