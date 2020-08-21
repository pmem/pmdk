/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2018-2020, Intel Corporation */

/*
 * container_ravl.h -- internal definitions for ravl-based block container
 */

#ifndef LIBPMEMOBJ_CONTAINER_RAVL_H
#define LIBPMEMOBJ_CONTAINER_RAVL_H 1

#include "container.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_container *container_new_ravl(struct palloc_heap *heap);

#ifdef __cplusplus
}
#endif

#endif /* LIBPMEMOBJ_CONTAINER_RAVL_H */
