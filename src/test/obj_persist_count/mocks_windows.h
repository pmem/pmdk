/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of pmem functions
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmemobj
 * files, when compiled for the purpose of obj_persist_count test.
 * It would replace default implementation with mocked functions defined
 * in obj_persist_count.c.
 *
 * These defines could be also passed as preprocessor definitions.
 */

#ifndef WRAP_REAL
#define pmem_persist __wrap_pmem_persist
#define pmem_flush __wrap_pmem_flush
#define pmem_drain __wrap_pmem_drain
#define pmem_msync __wrap_pmem_msync
#define pmem_memcpy_persist __wrap_pmem_memcpy_persist
#define pmem_memcpy_nodrain __wrap_pmem_memcpy_nodrain
#define pmem_memcpy __wrap_pmem_memcpy
#define pmem_memmove_persist __wrap_pmem_memmove_persist
#define pmem_memmove_nodrain __wrap_pmem_memmove_nodrain
#define pmem_memmove __wrap_pmem_memmove
#define pmem_memset_persist __wrap_pmem_memset_persist
#define pmem_memset_nodrain __wrap_pmem_memset_nodrain
#define pmem_memset __wrap_pmem_memset
#endif
