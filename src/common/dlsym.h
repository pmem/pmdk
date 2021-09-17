/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2021, Intel Corporation */

/*
 * dlsym.h -- dynamic linking utilities with library-specific implementation
 */

#ifndef PMDK_DLSYM_H
#define PMDK_DLSYM_H 1

#include "out.h"

#if defined(USE_LIBDL) && !defined(_WIN32)

#include <dlfcn.h>

/*
 * util_dlopen -- calls real dlopen()
 */
static inline void *
util_dlopen(const char *filename)
{
	LOG(3, "filename %s", filename);

	return dlopen(filename, RTLD_NOW);
}

/*
 * util_dlerror -- calls real dlerror()
 */
static inline char *
util_dlerror(void)
{
	return dlerror();
}

/*
 * util_dlsym -- calls real dlsym()
 */
static inline void *
util_dlsym(void *handle, const char *symbol)
{
	LOG(3, "handle %p symbol %s", handle, symbol);

	return dlsym(handle, symbol);
}

/*
 * util_dlclose -- calls real dlclose()
 */
static inline int
util_dlclose(void *handle)
{
	LOG(3, "handle %p", handle);

	return dlclose(handle);
}

#else /* empty functions */

/*
 * util_dlopen -- empty function
 */
static inline void *
util_dlopen(const char *filename)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(filename);

	errno = ENOSYS;
	return NULL;
}

/*
 * util_dlerror -- empty function
 */
static inline char *
util_dlerror(void)
{
	errno = ENOSYS;
	return NULL;
}

/*
 * util_dlsym -- empty function
 */
static inline void *
util_dlsym(void *handle, const char *symbol)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(handle, symbol);

	errno = ENOSYS;
	return NULL;
}

/*
 * util_dlclose -- empty function
 */
static inline int
util_dlclose(void *handle)
{
	/* suppress unused-parameter errors */
	SUPPRESS_UNUSED(handle);

	errno = ENOSYS;
	return 0;
}

#endif

#endif
