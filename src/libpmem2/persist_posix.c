/*
 * Copyright 2019-2020, Intel Corporation
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
