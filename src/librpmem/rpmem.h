/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2021, Intel Corporation */

/*
 * rpmem.h -- internal definitions for librpmem
 */
#include "alloc.h"
#include "fault_injection.h"

#define RPMEM_LOG_PREFIX "librpmem"
#define RPMEM_LOG_LEVEL_VAR "RPMEM_LOG_LEVEL"
#define RPMEM_LOG_FILE_VAR "RPMEM_LOG_FILE"

#if FAULT_INJECTION
void
rpmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at);

int
rpmem_fault_injection_enabled(void);
#else
static inline void
rpmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(type, nth, at);

	abort();
}

static inline int
rpmem_fault_injection_enabled(void)
{
	return 0;
}
#endif
