// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * mocks_windows.h -- redefinitions of obj list functions
 *
 * This file is Windows-specific.
 *
 * This file should be included (i.e. using Forced Include) by libpmemobj
 * files, when compiled for the purpose of obj_list test.
 * It would replace default implementation with mocked functions defined
 * in obj_list.c.
 *
 * These defines could be also passed as preprocessor definitions.
 */

#if defined(__cplusplus)
extern "C" {
#endif

#ifdef WRAP_REAL
#define WRAP_REAL_PMALLOC
#define WRAP_REAL_ULOG
#define WRAP_REAL_LANE
#define WRAP_REAL_HEAP
#define WRAP_REAL_PMEMOBJ
#endif

#ifndef WRAP_REAL_PMALLOC
#define pmalloc __wrap_pmalloc
#define pfree __wrap_pfree
#define pmalloc_construct __wrap_pmalloc_construct
#define prealloc __wrap_prealloc
#define prealloc_construct __wrap_prealloc_construct
#define palloc_usable_size __wrap_palloc_usable_size
#define palloc_reserve __wrap_palloc_reserve
#define palloc_publish __wrap_palloc_publish
#define palloc_defer_free __wrap_palloc_defer_free
#endif

#ifndef WRAP_REAL_ULOG
#define ulog_store __wrap_ulog_store
#define ulog_process __wrap_ulog_process
#endif

#ifndef WRAP_REAL_LANE
#define lane_hold __wrap_lane_hold
#define lane_release __wrap_lane_release
#define lane_recover_and_section_boot __wrap_lane_recover_and_section_boot
#define lane_section_cleanup __wrap_lane_section_cleanup
#endif

#ifndef WRAP_REAL_HEAP
#define heap_boot __wrap_heap_boot
#endif

#ifndef WRAP_REAL_PMEMOBJ
#define pmemobj_alloc __wrap_pmemobj_alloc
#define pmemobj_alloc_usable_size __wrap_pmemobj_alloc_usable_size
#define pmemobj_openU __wrap_pmemobj_open
#define pmemobj_close __wrap_pmemobj_close
#define pmemobj_direct __wrap_pmemobj_direct
#define pmemobj_pool_by_oid __wrap_pmemobj_pool_by_oid
#define pmemobj_pool_by_ptr __wrap_pmemobj_pool_by_ptr
#endif

#if defined(__cplusplus)
}
#endif
