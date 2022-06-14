/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2021, Intel Corporation */

/*
 * runtime.h - public definitions for a simple implementation of an asynchronous
 * runtime.
 *
 * This runtime is meant to be used together with concrete implementations
 * of the future abstract type. It will multiplex execution of the provided
 * array of futures, polling them in the current working thread until
 * all are complete.
 *
 * This implementation also provides a simple waker for futures that support it.
 * This means that the runtime will context switch if no futures can
 * make progress. There's no support for multi-threaded task scheduling.
 */

#ifndef RUNTIME_H
#define RUNTIME_H 1

#include "future.h"

#ifdef __cplusplus
extern "C" {
#endif

struct runtime;

struct runtime *runtime_new(void);
void runtime_delete(struct runtime *runtime);

void runtime_wait_multiple(struct runtime *runtime, struct future *futs[],
			size_t nfuts);

void runtime_wait(struct runtime *runtime, struct future *fut);

#ifdef __cplusplus
}
#endif
#endif /* RUNTIME_H */
