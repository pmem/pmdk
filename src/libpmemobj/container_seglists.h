// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2018, Intel Corporation */

/*
 * container_seglists.h -- internal definitions for
 *	segregated lists block container
 */

#ifndef LIBPMEMOBJ_CONTAINER_SEGLISTS_H
#define LIBPMEMOBJ_CONTAINER_SEGLISTS_H 1

#include "container.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_container *container_new_seglists(struct palloc_heap *heap);

#ifdef __cplusplus
}
#endif

#endif /* LIBPMEMOBJ_CONTAINER_SEGLISTS_H */
