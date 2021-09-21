// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include "future.h"

struct runtime;

struct runtime *runtime_new(void);
void runtime_delete(struct runtime *runtime);

void runtime_wait_multiple(struct runtime *runtime, struct future *futs[],
			size_t nfuts);

void runtime_wait(struct runtime *runtime, struct future *fut);

#endif
