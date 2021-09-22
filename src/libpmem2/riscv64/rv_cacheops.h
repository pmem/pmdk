/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

#ifndef RISCV64_CACHEOPS_H
#define RISCV64_CACHEOPS_H

#include <stdlib.h>

static inline void
riscv_store_memory_barrier(void)
{
	asm volatile("fence w,w" : : : "memory");
}
#endif
