/*
 * Copyright 2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * ut_lazy_load.c -- support for lazy loading of libraries
 */

#include "unittest.h"

#ifndef _WIN32

#include <dlfcn.h>

/*
 * ut_libopen -- open shared object
 */
void *
ut_libopen(const char *filename)
{
	void *handle = dlopen(filename, RTLD_LAZY);
	if (handle == NULL)
		UT_FATAL("dlopen: %s", dlerror());
	return handle;
}

/*
 * ut_libclose -- close shared object
 */
void
ut_libclose(void *handle)
{
	if (dlclose(handle) != 0)
		UT_FATAL("dlclose: %s", dlerror());
}

/*
 * ut_libsym -- obtain address of a symbol in a shared object
 */
void *
ut_libsym(void *handle, const char *symbol)
{
	void *sym = dlsym(handle, symbol);
	if (sym == NULL)
		UT_FATAL("dlclose: %s", dlerror());
	return sym;
}

#else

/*
 * ut_libopen -- open shared object
 */
HMODULE
ut_libopen(const char *filename)
{
	HMODULE handle = LoadLibrary(filename);
	if (handle == NULL)
		UT_FATAL("LoadLibrary, gle: %x", GetLastError());
	return handle;
}

/*
 * ut_libclose -- close shared object
 */
void
ut_libclose(HMODULE handle)
{
	if (FreeLibrary(handle) == FALSE)
		UT_FATAL("FreeLibrary, gle: %x", GetLastError());
}

/*
 * ut_libsym -- obtain address of a symbol in a shared object
 */
void *
ut_libsym(HMODULE handle, const char *symbol)
{
	void *sym = GetProcAddress(handle, symbol);
	if (sym == NULL)
		UT_FATAL("GetProcAddress, gle: %x", GetLastError());
	return sym;
}

#endif
