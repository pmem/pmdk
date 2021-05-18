/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2021, Intel Corporation */

/*
 * bucket.h -- internal definitions for bucket
 */

#ifndef LIBPMEMOBJ_BUCKET_H
#define LIBPMEMOBJ_BUCKET_H 1

#include <stddef.h>
#include <stdint.h>

#include "alloc_class.h"
#include "container.h"
#include "memblock.h"
#include "os_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CALC_SIZE_IDX(_unit_size, _size)\
	((_size) == 0 ? 0 : (uint32_t)((((_size)-1) / (_unit_size)) + 1))

struct bucket_locked;
struct bucket;

struct bucket_locked *bucket_locked_new(struct block_container *c,
					struct alloc_class *aclass);

struct bucket *bucket_acquire(struct bucket_locked *b);
void bucket_release(struct bucket *b);

struct alloc_class *bucket_alloc_class(struct bucket *b);
int *bucket_current_resvp(struct bucket *b);
int bucket_insert_block(struct bucket *b, const struct memory_block *m);
int bucket_try_insert_attached_block(struct bucket *b,
	const struct memory_block *m);
int bucket_remove_block(struct bucket *b, const struct memory_block *m);
int bucket_alloc_block(struct bucket *b, struct memory_block *m_out);

int bucket_attach_run(struct bucket *b, const struct memory_block *m);
int bucket_detach_run(struct bucket *b,
	struct memory_block *m_out, int *empty);

struct memory_block_reserved *bucket_active_block(struct bucket *b);

void bucket_locked_delete(struct bucket_locked *b);

#ifdef __cplusplus
}
#endif

#endif
