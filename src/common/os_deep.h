// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * os_deep.h -- abstraction layer for common usage of deep_* functions
 */

#ifndef PMDK_OS_DEEP_PERSIST_H
#define PMDK_OS_DEEP_PERSIST_H 1

#include <stdint.h>
#include <stddef.h>
#include "set.h"

#ifdef __cplusplus
extern "C" {
#endif

int os_range_deep_common(uintptr_t addr, size_t len);
int os_part_deep_common(struct pool_replica *rep, unsigned partidx, void *addr,
			size_t len, int flush);

#ifdef __cplusplus
}
#endif

#endif
