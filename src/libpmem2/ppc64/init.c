// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, IBM Corporation */
/* Copyright 2019-2020, Intel Corporation */

#include <errno.h>
#include <sys/mman.h>

#include "out.h"
#include "pmem2_arch.h"
#include "util.h"

/*
 * Older assemblers versions do not support the latest versions of L, e.g.
 * Binutils 2.34.
 * Workaround this by using longs.
 */
#define __SYNC(l) ".long (0x7c0004AC | ((" #l ") << 21))"
#define __DCBF(ra, rb, l) ".long (0x7c0000AC | ((" #l ") << 21)"	\
	" | ((" #ra ") << 16) | ((" #rb ") << 11))"

static void
ppc_fence(void)
{
	LOG(15, NULL);

	/*
	 * Force a memory barrier to flush out all cache lines.
	 * Uses a heavyweight sync in order to guarantee the memory ordering
	 * even with a data cache flush.
	 * According to the POWER ISA 3.1, phwsync (aka. sync (L=4)) is treated
	 * as a hwsync by processors compatible with previous versions of the
	 * POWER ISA.
	 */
	asm volatile(__SYNC(4) : : : "memory");
}

static void
ppc_fence_empty(void)
{
	LOG(15, NULL);
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
		/*
		 * Flush the data cache block.
		 * According to the POWER ISA 3.1, dcbstps (aka. dcbf (L=6))
		 * behaves as dcbf (L=0) on previous processors.
		 */
		asm volatile(__DCBF(0, %0, 6) : :"r"(uptr) : "memory");

		uptr += CACHELINE_SIZE;
	}
}

static void
ppc_flush_msync(const void *addr, size_t size)
{
	LOG(15, "addr %p len %zu", addr, len);
	/* this implementation is copy of pmem_msync */

	VALGRIND_DO_CHECK_MEM_IS_ADDRESSABLE(addr, len);

	/*
	 * msync requires addr to be a multiple of pagesize but there are no
	 * requirements for len. Align addr down and change len so that
	 * [addr, addr + len) still contains initial range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~((uintptr_t)Pagesize - 1);

	/*
	 * msync accepts addresses aligned to page boundary, so we may sync
	 * more and part of it may have been marked as undefined/inaccessible
	 * Msyncing such memory is not a bug, so as a workaround temporarily
	 * disable error reporting.
	 */
	VALGRIND_DO_DISABLE_ERROR_REPORTING;

	int ret;
	if ((ret = msync((void *)uptr, len, MS_SYNC)) < 0)
		ERR("!msync");

	VALGRIND_DO_ENABLE_ERROR_REPORTING;

	/* full flush */
	VALGRIND_DO_PERSIST(uptr, len);
}

void
pmem2_arch_init(struct pmem2_arch_info *info)
{
	LOG(3, "libpmem*: PPC64 support");
	LOG(3, "PMDK PPC64 support is currently experimental");
	LOG(3, "Please don't use this library in production environment");
	if (On_valgrind) {
		info->fence = ppc_fence_empty;
		info->flush = ppc_flush_msync;
	} else {
		info->fence = ppc_fence;
		info->flush = ppc_flush;
	}
}
