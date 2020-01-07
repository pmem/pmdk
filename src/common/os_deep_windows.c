// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * os_deep_windows.c -- Windows abstraction layer for deep_* functions
 */

#include <windows.h>
#include "out.h"
#include "os.h"
#include "set.h"
#include "libpmem.h"

/*
 * os_range_deep_common -- call msnyc for non DEV dax
 */
int
os_range_deep_common(uintptr_t addr, size_t len)
{
	LOG(3, "os_range_deep_common addr %p len %lu", addr, len);

	if (len == 0)
		return 0;
	return pmem_msync((void *)addr, len);
}

/*
 * os_part_deep_common -- common function to handle both
 * deep_persist and deep_drain part flush cases.
 */
int
os_part_deep_common(struct pool_replica *rep, unsigned partidx, void *addr,
			size_t len, int flush)
{
	LOG(3, "part %p part %d addr %p len %lu flush %d",
		rep, partidx, addr, len, flush);

	if (!rep->is_pmem) {
		/*
		 * In case of part on non-pmem call msync on the range
		 * to deep flush the data. Deep drain is empty as all
		 * data is msynced to persistence.
		 */

		if (!flush)
			return 0;

		if (pmem_msync(addr, len)) {
			LOG(1, "pmem_msync(%p, %lu)", addr, len);
			return -1;
		}
		return 0;
	}

	/* Call deep flush if it was requested */
	if (flush) {
		LOG(15, "pmem_deep_flush addr %p, len %lu", addr, len);
		pmem_deep_flush(addr, len);
	}
	/*
	 * Before deep drain call normal drain to ensure that data
	 * is at least in WPQ.
	 */
	pmem_drain();

	/*
	 * For deep_drain on normal pmem it is enough to
	 * call msync on one page.
	 */
	if (pmem_msync(addr, MIN(Pagesize, len))) {
		LOG(1, "pmem_msync(%p, %lu)", addr, len);
		return -1;
	}
	return 0;
}
