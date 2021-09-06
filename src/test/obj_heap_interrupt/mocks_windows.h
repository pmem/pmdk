/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of memops functions
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmemobj
 * files, when compiled for the purpose of obj_heap_interrupt test.
 * It would replace default implementation with mocked functions defined
 * in obj_heap_interrupt.c.
 *
 * These defines could be also passed as preprocessor definitions.
 */

#ifndef WRAP_REAL
#define operation_finish __wrap_operation_finish
#endif
