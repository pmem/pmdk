/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

/*
 * membuf.h -- definitions for "membuf" module.
 *
 * Membuf is a circular object buffer. Each instance uses an internal
 * per-thread buffer to avoid heavyweight synchronization.
 *
 * Allocation is linear and very cheap. The expectation is that objects within
 * the buffer will be reclaimable long before the linear allocator might need
 * to wraparound to reuse memory.
 */

#ifndef MEMBUF_H
#define MEMBUF_H

#include <stddef.h>

struct membuf;

struct membuf *membuf_new(void *user_data);
void membuf_delete(struct membuf *membuf);

void *membuf_alloc(struct membuf *membuf, size_t size);
void membuf_free(void *ptr);

void *membuf_ptr_user_data(void *ptr);

#endif
