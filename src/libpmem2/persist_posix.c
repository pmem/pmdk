/*
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

/*
 * persist_posix.c -- POSIX-specific part of persist implementation
 */

#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>

#include "out.h"
#include "persist.h"
#include "valgrind_internal.h"

/*
 * pmem2_persist_pages_internal -- flush processor cache for the given range
 */
static int
pmem2_persist_pages_internal(const void *addr, size_t len, int restart_on_eintr)
{
	pmem2_log_flush(addr, len);

	/*
	 * msync requires addr to be a multiple of pagesize but there are no
	 * requirements for len. Align addr down and change len so that
	 * [addr, addr + len) still contains initial range.
	 */

	/* increase len by the amount we gain when we round addr down */
	len += (uintptr_t)addr & (Pagesize - 1);

	/* round addr down to page boundary */
	uintptr_t uptr = (uintptr_t)addr & ~((uintptr_t)Pagesize - 1);

	int olderrno = errno;
	/*
	 * msync accepts addresses aligned to page boundary, so we may sync
	 * more and part of it may have been marked as undefined/inaccessible
	 * Msyncing such memory is not a bug, so as a workaround temporarily
	 * disable error reporting.
	 */
	VALGRIND_DO_DISABLE_ERROR_REPORTING;

	int ret;
	do {
		ret = msync((void *)uptr, len, MS_SYNC);

		if (ret < 0)
			ERR("!msync");
	} while (restart_on_eintr && ret < 0 && errno == EINTR);

	VALGRIND_DO_ENABLE_ERROR_REPORTING;

	errno = olderrno;
	if (ret)
		return ret;

	/* full flush */
	VALGRIND_DO_PERSIST(uptr, len);

	return 0;
}

/*
 * pmem2_persist_pages -- flush processor cache for the given range
 */
void
pmem2_persist_pages(const void *addr, size_t len)
{
	/*
	 * Restarting on EINTR in general is a bad idea, but we currently
	 * don't have any way to communicate the failure outside.
	 */
	const int restart_on_eintr = 1;

	int ret = pmem2_persist_pages_internal(addr, len, restart_on_eintr);
	if (ret)
		/*
		 * msync failure means that the address was not mapped.
		 * This is severe enough to trigger crash.
		 */
		abort();
}
