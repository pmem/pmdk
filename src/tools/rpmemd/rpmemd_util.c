/*
 * Copyright 2017, Intel Corporation
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
 * rpmemd_util.c -- rpmemd utility functions definitions
 */

#include <stdlib.h>
#include <unistd.h>

#include "libpmem.h"
#include "rpmem_common.h"
#include "rpmemd_log.h"
#include "rpmemd_util.h"

/*
 * rpmemd_pmem_persist -- pmem_persist wrapper required to unify function
 * pointer type with pmem_msync
 */
int
rpmemd_pmem_persist(const void *addr, size_t len)
{
	pmem_persist(addr, len);
	return 0;
}

/*
 * rpmemd_flush_fatal -- APM specific flush function which should never be
 * called because APM does not require flushes
 */
int
rpmemd_flush_fatal(const void *addr, size_t len)
{
	RPMEMD_FATAL("rpmemd_flush_fatal should never be called");
}

/*
 * rpmemd_persist_to_str -- convert persist function pointer to string
 */
static const char *
rpmemd_persist_to_str(int (*persist)(const void *addr, size_t len))
{
	if (persist == rpmemd_pmem_persist) {
		return "pmem_persist";
	} else if (persist == pmem_msync) {
		return "pmem_msync";
	} else if (persist == rpmemd_flush_fatal) {
		return "none";
	} else {
		return NULL;
	}
}

/*
 * rpmem_print_pm_policy -- print persistency method policy
 */
static void
rpmem_print_pm_policy(enum rpmem_persist_method persist_method,
		int (*persist)(const void *addr, size_t len))
{
	RPMEMD_LOG(NOTICE, "\tpersist method: %s",
			rpmem_persist_method_to_str(persist_method));
	RPMEMD_LOG(NOTICE, "\tpersist flush: %s",
			rpmemd_persist_to_str(persist));
}

/*
 * rpmemd_apply_pm_policy -- choose the persistency method and the flush
 * function according to the pool type and the persistency method read from the
 * config
 */
int
rpmemd_apply_pm_policy(enum rpmem_persist_method *persist_method,
		int (**persist)(const void *addr, size_t len),
		const int is_pmem)
{
	switch (*persist_method) {
	case RPMEM_PM_APM:
		if (is_pmem) {
			*persist_method = RPMEM_PM_APM;
			*persist = rpmemd_flush_fatal;
		} else {
			*persist_method = RPMEM_PM_GPSPM;
			*persist = pmem_msync;
		}
		break;
	case RPMEM_PM_GPSPM:
		*persist_method = RPMEM_PM_GPSPM;
		*persist = is_pmem ? rpmemd_pmem_persist : pmem_msync;
		break;
	default:
		RPMEMD_FATAL("invalid persist method: %d", *persist_method);
		return -1;
	}

	RPMEMD_LOG(NOTICE, "persistency policy:");
	rpmem_print_pm_policy(*persist_method, *persist);

	return 0;
}
