/*
 * Copyright 2016, Intel Corporation
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
 * util_dl.h -- dynamic linking utilities with library-specific implementation
 */

#if defined(USE_LIBDL) && !defined(_WIN32)

#include <dlfcn.h>

/*
 * util_dlopen -- calls real dlopen()
 */
static inline void *
util_dlopen(const char *filename)
{
	return dlopen(filename, RTLD_NOW | RTLD_GLOBAL);
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
	return dlsym(handle, symbol);
}

/*
 * util_dlclose -- calls real dlclose()
 */
static inline int
util_dlclose(void *handle)
{
	return dlclose(handle);
}

#else /* empty functions */

/*
 * util_dlopen -- empty function
 */
static inline void *
util_dlopen(const char *filename)
{
	return NULL;
}

/*
 * util_dlerror -- empty function
 */
static inline char *
util_dlerror(void)
{
	return NULL;
}

/*
 * util_dlsym -- empty function
 */
static inline void *
util_dlsym(void *handle, const char *symbol)
{
	return NULL;
}

/*
 * util_dlclose -- empty function
 */
static inline int
util_dlclose(void *handle)
{
	return 0;
}

#endif
