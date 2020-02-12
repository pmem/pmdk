// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * bucket.h -- internal definitions for bucket
 */

#ifndef LIBPMEMOBJ_BUCKET_H
#define LIBPMEMOBJ_BUCKET_H 1

#include <stddef.h>
#include <stdint.h>

#include "container.h"
#include "memblock.h"
#include "os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CALC_SIZE_IDX(_unit_size, _size)\
((_size) == 0 ? 0 : (uint32_t)((((_size) - 1) / (_unit_size)) + 1))

struct bucket {
	os_mutex_t lock;

	struct alloc_class *aclass;

	struct block_container *container;
	const struct block_container_ops *c_ops;

	struct memory_block_reserved *active_memory_block;
	int is_active;
};

struct bucket *bucket_new(struct block_container *c,
	struct alloc_class *aclass);

int *bucket_current_resvp(struct bucket *b);

int bucket_insert_block(struct bucket *b, const struct memory_block *m);

void bucket_delete(struct bucket *b);

#ifdef __cplusplus
}
#endif

#endif
