// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

#ifndef COMMON_FAULT_INJECTION
#define COMMON_FAULT_INJECTION

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum pmem_allocation_type { PMEM_MALLOC, PMEM_REALLOC };

#if FAULT_INJECTION
void common_inject_fault_at(enum pmem_allocation_type type,
	int nth, const char *at);

int common_fault_injection_enabled(void);

#else
static inline void
common_inject_fault_at(enum pmem_allocation_type type, int nth, const char *at)
{
	abort();
}

static inline int
common_fault_injection_enabled(void)
{
	return 0;
}
#endif

#ifdef __cplusplus
}
#endif

#endif
