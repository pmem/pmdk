/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2017-2024, Intel Corporation */

/*
 * util_pmem.h -- internal definitions for pmem utils
 */

#ifndef PMDK_UTIL_PMEM_H
#define PMDK_UTIL_PMEM_H 1

#include "libpmem.h"
#include "out.h"
#include "log_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * util_persist -- flush to persistence
 */
static inline void
util_persist(int is_pmem, const void *addr, size_t len)
{
	LOG(3, "is_pmem %d, addr %p, len %zu", is_pmem, addr, len);

	if (is_pmem)
		pmem_persist(addr, len);
	else if (pmem_msync(addr, len))
		CORE_LOG_FATAL_W_ERRNO("pmem_msync");
}

/*
 * util_persist_auto -- flush to persistence
 */
static inline void
util_persist_auto(int is_pmem, const void *addr, size_t len)
{
	LOG(3, "is_pmem %d, addr %p, len %zu", is_pmem, addr, len);

	util_persist(is_pmem || pmem_is_pmem(addr, len), addr, len);
}

#ifdef __cplusplus
}
#endif

#endif /* util_pmem.h */
