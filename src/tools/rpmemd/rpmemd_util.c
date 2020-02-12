// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

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
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "persist method: %s",
			rpmem_persist_method_to_str(persist_method));
	RPMEMD_LOG(NOTICE, RPMEMD_LOG_INDENT "persist flush: %s",
			rpmemd_persist_to_str(persist));
}

/*
 * rpmem_memcpy_msync -- memcpy and msync
 */
static void *
rpmem_memcpy_msync(void *pmemdest, const void *src, size_t len)
{
	void *ret = pmem_memcpy(pmemdest, src, len, PMEM_F_MEM_NOFLUSH);
	pmem_msync(pmemdest, len);

	return ret;
}

/*
 * rpmemd_apply_pm_policy -- choose the persistency method and the flush
 * function according to the pool type and the persistency method read from the
 * config
 */
int
rpmemd_apply_pm_policy(enum rpmem_persist_method *persist_method,
	int (**persist)(const void *addr, size_t len),
	void *(**memcpy_persist)(void *pmemdest, const void *src, size_t len),
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

	/* this is for RPMEM_PERSIST_INLINE */
	if (is_pmem)
		*memcpy_persist = pmem_memcpy_persist;
	else
		*memcpy_persist = rpmem_memcpy_msync;

	RPMEMD_LOG(NOTICE, "persistency policy:");
	rpmem_print_pm_policy(*persist_method, *persist);

	return 0;
}
