/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2021, Intel Corporation */

/*
 * container.h -- internal definitions for block containers
 */

#ifndef LIBPMEMOBJ_CONTAINER_H
#define LIBPMEMOBJ_CONTAINER_H 1

#include "memblock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct block_container {
	const struct block_container_ops *c_ops;
};

struct block_container_ops {
	/* inserts a new memory block into the container */
	int (*insert)(struct block_container *c, const struct memory_block *m);

	/* removes exact match memory block */
	int (*get_rm_exact)(struct block_container *c,
		const struct memory_block *m);

	/* removes and returns the best-fit memory block for size */
	int (*get_rm_bestfit)(struct block_container *c,
		struct memory_block *m);

	/* checks whether the container is empty */
	int (*is_empty)(struct block_container *c);

	/* removes all elements from the container */
	void (*rm_all)(struct block_container *c);

	/* deletes the container */
	void (*destroy)(struct block_container *c);
};

#ifdef __cplusplus
}
#endif

#endif /* LIBPMEMOBJ_CONTAINER_H */
