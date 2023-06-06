// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2023, Intel Corporation */

/*
 * ut_mt.c -- implementation of the multithread worker interface
 */

#include "unittest.h"
#include "ut_mt.h"

void
run_workers(void *(worker_func)(void *arg), unsigned threads, void *args[])
{
	os_thread_t *t = malloc(threads * sizeof(os_thread_t));

	for (unsigned i = 0; i < threads; ++i)
		THREAD_CREATE(&t[i], NULL, worker_func, args[i]);

	for (unsigned i = 0; i < threads; ++i)
		THREAD_JOIN(&t[i], NULL);
	free(t);
}
