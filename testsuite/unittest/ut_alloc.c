// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * ut_alloc.c -- unit test memory allocation routines
 */

#include "unittest.h"

/*
 * ut_malloc -- a malloc that cannot return NULL
 */
void *
ut_malloc(const char *file, int line, const char *func, size_t size)
{
	void *retval = malloc(size);

	if (retval == NULL)
		ut_fatal(file, line, func, "cannot malloc %zu bytes", size);

	return retval;
}

/*
 * ut_calloc -- a calloc that cannot return NULL
 */
void *
ut_calloc(const char *file, int line, const char *func,
	size_t nmemb, size_t size)
{
	void *retval = calloc(nmemb, size);

	if (retval == NULL)
		ut_fatal(file, line, func, "cannot calloc %zu bytes", size);

	return retval;
}

/*
 * ut_free -- wrapper for free
 *
 * technically we don't need to wrap free since there's no return to
 * check.  using this wrapper to add memory allocation tracking later.
 */
void
ut_free(const char *file, int line, const char *func, void *ptr)
{
	free(ptr);
}

/*
 * ut_aligned_free -- wrapper for aligned memory free
 */
void
ut_aligned_free(const char *file, int line, const char *func, void *ptr)
{
#ifndef _WIN32
	free(ptr);
#else
	_aligned_free(ptr);
#endif
}

/*
 * ut_realloc -- a realloc that cannot return NULL
 */
void *
ut_realloc(const char *file, int line, const char *func,
	void *ptr, size_t size)
{
	void *retval = realloc(ptr, size);

	if (retval == NULL)
		ut_fatal(file, line, func, "cannot realloc %zu bytes", size);

	return retval;
}

/*
 * ut_strdup -- a strdup that cannot return NULL
 */
char *
ut_strdup(const char *file, int line, const char *func,
	const char *str)
{
	char *retval = strdup(str);

	if (retval == NULL)
		ut_fatal(file, line, func, "cannot strdup %zu bytes",
			strlen(str));

	return retval;
}

/*
 * ut_memalign -- like malloc but page-aligned memory
 */
void *
ut_memalign(const char *file, int line, const char *func, size_t alignment,
    size_t size)
{
	void *retval;

#ifndef _WIN32
	if ((errno = posix_memalign(&retval, alignment, size)) != 0)
		ut_fatal(file, line, func,
		    "!memalign %zu bytes (%zu alignment)", size, alignment);
#else
	retval = _aligned_malloc(size, alignment);
	if (!retval) {
		ut_fatal(file, line, func,
			"!memalign %zu bytes (%zu alignment)", size, alignment);
	}
#endif

	return retval;
}

/*
 * ut_pagealignmalloc -- like malloc but page-aligned memory
 */
void *
ut_pagealignmalloc(const char *file, int line, const char *func,
    size_t size)
{
	return ut_memalign(file, line, func, (size_t)Ut_pagesize, size);
}

/*
 * ut_mmap_anon_aligned -- mmaps anonymous memory with specified (power of two,
 *                         multiple of page size) alignment and adds guard
 *                         pages around it
 */
void *
ut_mmap_anon_aligned(const char *file, int line, const char *func,
	size_t alignment, size_t size)
{
	char *d, *d_aligned;
	uintptr_t di, di_aligned;
	size_t sz;

	if (alignment == 0)
		alignment = Ut_mmap_align;

	/* alignment must be a multiple of page size */
	if (alignment & (Ut_mmap_align - 1))
		return NULL;

	/* power of two */
	if (alignment & (alignment - 1))
		return NULL;

	d = ut_mmap(file, line, func, NULL, size + 2 * alignment,
		PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	di = (uintptr_t)d;
	di_aligned = (di + alignment - 1) & ~(alignment - 1);

	if (di == di_aligned)
		di_aligned += alignment;
	d_aligned = (void *)di_aligned;

	sz = di_aligned - di;
	if (sz - Ut_mmap_align)
		ut_munmap(file, line, func, d, sz - Ut_mmap_align);

	/* guard page before */
	ut_mprotect(file, line, func,
		d_aligned - Ut_mmap_align, Ut_mmap_align, PROT_NONE);

	/* guard page after */
	ut_mprotect(file, line, func,
		d_aligned + size, Ut_mmap_align, PROT_NONE);

	sz = di + size + 2 * alignment - (di_aligned + size) - Ut_mmap_align;
	if (sz)
		ut_munmap(file, line, func,
			d_aligned + size + Ut_mmap_align, sz);

	return d_aligned;
}

/*
 * ut_munmap_anon_aligned -- unmaps anonymous memory allocated by
 *                           ut_mmap_anon_aligned
 */
int
ut_munmap_anon_aligned(const char *file, int line, const char *func,
	void *start, size_t size)
{
	return ut_munmap(file, line, func, (char *)start - Ut_mmap_align,
		size + 2 * Ut_mmap_align);
}
