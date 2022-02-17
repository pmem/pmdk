/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2022, Intel Corporation */

#ifndef DATA_MOVER_THREADS_H
#define DATA_MOVER_THREADS_H

#include "vdm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct data_mover_threads;

struct data_mover_threads *data_mover_threads_new(size_t nthreads,
	size_t ringbuf_size, enum future_notifier_type desired_notifier);
struct data_mover_threads *data_mover_threads_default();

struct vdm *data_mover_threads_get_vdm(struct data_mover_threads *dmt);

void data_mover_threads_delete(struct data_mover_threads *dmt);

#ifdef __cplusplus
}
#endif
#endif /* DATA_MOVER_THREADS_H */
