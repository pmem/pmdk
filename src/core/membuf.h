/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

/*
 * membuf.h -- definitions for "membuf" module.
 *
 * Membuf is a circular object buffer with automatic reclamation. Each instance
 * uses an internal per-thread buffer to avoid heavyweight synchronization.
 *
 * Allocation is linear and very cheap. The expectation is that objects within
 * the buffer will be reclaimable long before the linear allocator might need
 * to wraparound to reuse memory.
 */

#ifndef MEMBUF_H
#define MEMBUF_H

#include <stddef.h>

struct membuf;

enum membuf_check_result {
	/*
	 * Cannot reclaim memory object, alloc will fail. This should be used
	 * when object is owned by the current working thread.
	 */
	MEMBUF_PTR_IN_USE,

	/*
	 * Cannot reclaim memory object, alloc will busy-poll. This should be
	 * used when object is being processed in the background.
	 */
	MEMBUF_PTR_CAN_WAIT,

	/*
	 * Can reclaim memory object, alloc will reuse memory.
	 */
	MEMBUF_PTR_CAN_REUSE,
};

typedef enum membuf_check_result (*membuf_ptr_check)(void *ptr, void *data);
typedef size_t (*membuf_ptr_size)(void *ptr, void *data);

struct membuf *membuf_new(membuf_ptr_check check_func,
	membuf_ptr_size size_func,
	void *func_data, void *user_data);
void membuf_delete(struct membuf *membuf);

void *membuf_alloc(struct membuf *membuf, size_t size);

void *membuf_ptr_user_data(void *ptr);

#endif
