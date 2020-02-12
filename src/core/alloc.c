// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include <errno.h>

#include "alloc.h"
#include "fault_injection.h"
#include "out.h"

Malloc_func fn_malloc = malloc;
Realloc_func fn_realloc = realloc;

#if FAULT_INJECTION
static __thread int malloc_num;
static __thread int fail_malloc_num;
static __thread const char *fail_malloc_from;

void *
_flt_Malloc(size_t size, const char *func)
{
	if (fail_malloc_from && strcmp(func, fail_malloc_from) == 0) {
		if (++malloc_num == fail_malloc_num) {
			errno = ENOMEM;
			return NULL;
		}
	}
	return fn_malloc(size);
}

static __thread int realloc_num;
static __thread int fail_realloc_num;
static __thread const char *fail_realloc_from;

void *
_flt_Realloc(void *ptr, size_t size, const char *func)
{
	if (fail_realloc_from && strcmp(func, fail_realloc_from) == 0) {
		if (++realloc_num == fail_realloc_num) {
			errno = ENOMEM;
			return NULL;
		}
	}
	return fn_realloc(ptr, size);
}

void
core_inject_fault_at(enum pmem_allocation_type type, int nth, const char *at)
{
	switch (type) {
		case PMEM_MALLOC:
			malloc_num = 0;
			fail_malloc_num = nth;
			fail_malloc_from = at;
			break;
		case PMEM_REALLOC:
			realloc_num = 0;
			fail_realloc_num = nth;
			fail_realloc_from = at;
			break;
		default:
			FATAL("unknown allocation type");
	}
}

int
core_fault_injection_enabled(void)
{
	return 1;
}
#else
void *_Malloc(size_t size) {
	return fn_malloc(size);
}

void *_Realloc(void *ptr, size_t size) {
	return fn_realloc(ptr, size);
}
#endif

void set_func_malloc(void *(*malloc_func)(size_t size)) {
	fn_malloc = (malloc_func == NULL) ? malloc : malloc_func;
}

void set_func_realloc(void *(*realloc_func)(void *ptr, size_t size)) {
	fn_realloc = (realloc_func == NULL) ? realloc : realloc_func;
}

/*
 * our versions of malloc & friends start off pointing to the libc versions
 */
Free_func Free = free;
Strdup_func Strdup = strdup;

/*
 * Zalloc -- allocate zeroed memory
 */
void *
Zalloc(size_t sz)
{
	void *ret = Malloc(sz);
	if (!ret)
		return NULL;
	return memset(ret, 0, sz);
}

/*
 * util_set_alloc_funcs -- allow one to override malloc, etc.
 */
void
util_set_alloc_funcs(void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s))
{
	set_func_malloc(malloc_func);
	Free = (free_func == NULL) ? free : free_func;
	set_func_realloc(realloc_func);
	Strdup = (strdup_func == NULL) ? strdup : strdup_func;
}
