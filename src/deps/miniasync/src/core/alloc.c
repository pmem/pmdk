// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2022, Intel Corporation */

#include <errno.h>

#include "alloc.h"
#include "out.h"

Malloc_func fn_malloc = malloc;
Realloc_func fn_realloc = realloc;

void *_Malloc(size_t size) {
	return fn_malloc(size);
}

void *_Realloc(void *ptr, size_t size) {
	return fn_realloc(ptr, size);
}

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
