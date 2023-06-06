/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2023, Intel Corporation */

/*
 * ut_mt.h -- multithread worker interface
 */

#ifndef UT_MT_H
#define UT_MT_H 1

void
run_workers(void *(worker_func)(void *arg), unsigned threads, void *args[]);

#endif /* UT_MT_H */
