/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef LOONGARCH64_CACHEOPS_H
#define LOONGARCH64_CACHEOPS_H

#include <stdlib.h>

static inline void
loongarch_store_memory_barrier(void)
{
	asm volatile("dbar 0" : : : "memory");
}
#endif
