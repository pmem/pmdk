/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2023, Intel Corporation */

/*
 * ut_mt.h -- multithread worker interface
 */

#ifndef _UT_MT_H
#define _UT_MT_H 1

struct workers_args {
	void *args;
};

void
run_workers(void *(worker_func)(void *arg), unsigned threads,
		struct workers_args args[]);

#endif /* _UT_MT_H */
