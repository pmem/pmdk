/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef DATA_MOVER_THREADS_H
#define DATA_MOVER_THREADS_H

#include "vdm.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*memcpy_fn)(void *dst, const void *src,
				size_t n, unsigned flags);
typedef void *(*memmove_fn)(void *dst, const void *src,
				size_t n, unsigned flags);
typedef void *(*memset_fn)(void *str, int c, size_t n,
				unsigned flags);

struct data_mover_threads;
struct data_mover_threads *data_mover_threads_new(size_t nthreads,
	size_t ringbuf_size, enum future_notifier_type desired_notifier);
struct data_mover_threads *data_mover_threads_default();
struct vdm *data_mover_threads_get_vdm(struct data_mover_threads *dmt);
void data_mover_threads_delete(struct data_mover_threads *dmt);
void data_mover_threads_set_memcpy_fn(struct data_mover_threads *dmt,
	memcpy_fn op_memcpy);
void data_mover_threads_set_memmove_fn(struct data_mover_threads *dmt,
	memmove_fn op_memmove);
void data_mover_threads_set_memset_fn(struct data_mover_threads *dmt,
	memset_fn op_memset);

#ifdef __cplusplus
}
#endif
#endif /* DATA_MOVER_THREADS_H */
