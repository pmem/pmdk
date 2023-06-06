// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2023, Intel Corporation */

/*
 * ut_mt.c -- implementation of multithread worker interface
 */

#include "unittest.h"
#include "ut_mt.h"

void
run_workers(void *(worker_func)(void *arg), unsigned threads,
		struct workers_args args[])
{
	os_thread_t *t = malloc(threads * sizeof(os_thread_t));

	for (unsigned i = 0; i < threads; ++i)
		THREAD_CREATE(&t[i], NULL, worker_func, args[i].args);

	for (unsigned i = 0; i < threads; ++i)
		THREAD_JOIN(&t[i], NULL);
	free(t);
}
