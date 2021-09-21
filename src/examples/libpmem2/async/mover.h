// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#ifndef MOVER_H
#define MOVER_H 1

#include "future.h"

struct mover;

typedef void (*mover_cb_fn)(struct future_context *context);

struct mover_memcpy_data {
	struct future_waker waker;
	_Atomic int complete;
	struct mover *mover;
	void *dest;
	void *src;
	size_t n;
	mover_cb_fn mover_cb;
};

struct mover_memcpy_output {
	void *dest;
};

FUTURE(mover_memcpy_future,
	struct mover_memcpy_data, struct mover_memcpy_output);

struct mover_memcpy_future mover_memcpy(struct mover *mover,
	void *dest, void *src, size_t n);

typedef void (*async_memcpy_fn)(void *runner, struct future_context *context);

struct mover_runner {
	void *runner_data;
	async_memcpy_fn memcpy;
};

struct mover_runner *mover_runner_synchronous(void);
struct mover_runner *mover_runner_pthreads(void);

struct mover *mover_new(struct mover_runner *runner);

void mover_delete(struct mover *mover);

#endif
