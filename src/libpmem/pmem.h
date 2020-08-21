/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2014-2020, Intel Corporation */

/*
 * pmem.h -- internal definitions for libpmem
 */
#ifndef PMEM_H
#define PMEM_H

#include <stddef.h>
#include "alloc.h"
#include "fault_injection.h"
#include "util.h"
#include "valgrind_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PMEM_LOG_PREFIX "libpmem"
#define PMEM_LOG_LEVEL_VAR "PMEM_LOG_LEVEL"
#define PMEM_LOG_FILE_VAR "PMEM_LOG_FILE"

typedef int (*is_pmem_func)(const void *addr, size_t len);

void pmem_init(void);
void pmem_os_init(is_pmem_func *func);

int is_pmem_detect(const void *addr, size_t len);
void *pmem_map_register(int fd, size_t len, const char *path, int is_dev_dax);

#if FAULT_INJECTION
void
pmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at);

int
pmem_fault_injection_enabled(void);
#else
static inline void
pmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	abort();
}

static inline int
pmem_fault_injection_enabled(void)
{
	return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
