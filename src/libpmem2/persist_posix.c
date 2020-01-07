// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * persist_posix.c -- POSIX-specific part of persist implementation
 */

#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

#include "out.h"
#include "persist.h"
#include "pmem2_utils.h"
#include "valgrind_internal.h"

/*
 * pmem2_flush_file_buffers_os -- flush CPU and OS file caches for the given
 * range
 */
int
pmem2_flush_file_buffers_os(struct pmem2_map *map, const void *addr, size_t len,
		int autorestart)
{
	/*
	 * msync accepts addresses aligned to the page boundary, so we may sync
	 * more and part of it may have been marked as undefined/inaccessible.
	 * Msyncing such memory is not a bug, so as a workaround temporarily
	 * disable error reporting.
	 */
	VALGRIND_DO_DISABLE_ERROR_REPORTING;
	int ret;
	do {
		ret = msync((void *)addr, len, MS_SYNC);

		if (ret < 0) {
			ERR("!msync");
		} else {
			/* full flush */
			VALGRIND_DO_PERSIST((uintptr_t)addr, len);
		}
	} while (autorestart && ret < 0 && errno == EINTR);

	VALGRIND_DO_ENABLE_ERROR_REPORTING;

	if (ret)
		return PMEM2_E_ERRNO;

	return 0;
}
