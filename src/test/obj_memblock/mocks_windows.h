// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of memops functions
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmemobj
 * files, when compiled for the purpose of obj_memblock test.
 * It would replace default implementation with mocked functions defined
 * in obj_memblock.c.
 *
 * These defines could be also passed as preprocessor definitions.
 */

#ifndef WRAP_REAL
#define operation_add_typed_entry __wrap_operation_add_typed_entry
#define operation_add_entry __wrap_operation_add_entry
#endif
