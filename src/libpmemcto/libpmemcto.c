/*
 * Copyright 2016-2018, Intel Corporation
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
 * libpmemcto.c -- basic libpmemcto functions
 */
#include <stdio.h>
#include <stdint.h>

#include "libpmemcto.h"

#include "pmemcommon.h"
#include "cto.h"
#include "jemalloc.h"

static const char *Ver = LIBVERSTR(PMEMCTO_LOG_PREFIX,
		PMEMCTO_MAJOR_VERSION, PMEMCTO_MINOR_VERSION);

/*
 * libpmemcto_init -- load-time initialization for log
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmemcto_init(void)
{
	common_init(PMEMCTO_LOG_PREFIX, PMEMCTO_LOG_LEVEL_VAR,
			PMEMCTO_LOG_FILE_VAR, PMEMCTO_MAJOR_VERSION,
			PMEMCTO_MINOR_VERSION);
	cto_init();
	LOG(3, NULL);
}

/*
 * libpmemcto_fini -- libpmemcto cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmemcto_fini(void)
{
	LOG(3, NULL);
	cto_fini();
	common_fini();
}

/*
 * pmemcto_check_versionU -- see if lib meets application version requirements
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemcto_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required == 0 && minor_required == 0) {
		LOG(4, Ver);
		return Ver;
	}

	if (major_required != PMEMCTO_MAJOR_VERSION) {
		ERR("libpmemcto major version mismatch (need %u, found %u)",
			major_required, PMEMCTO_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > PMEMCTO_MINOR_VERSION) {
		ERR("libpmemcto minor version mismatch (need %u, found %u)",
			minor_required, PMEMCTO_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

#ifndef _WIN32
/*
 * pmemcto_check_version -- see if lib meets application version requirements
 */
const char *
pmemcto_check_version(unsigned major_required, unsigned minor_required)
{
	return pmemcto_check_versionU(major_required, minor_required);
}
#else
/*
 * pmemcto_check_versionW -- see if lib meets application version requirements
 */
const wchar_t *
pmemcto_check_versionW(unsigned major_required, unsigned minor_required)
{
	if (pmemcto_check_versionU(major_required, minor_required) != NULL)
		return out_get_errormsgW();
	else
		return NULL;
}
#endif

/*
 * pmemcto_set_funcs -- allow overriding libpmemcto's call to malloc, etc.
 */
void
pmemcto_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s),
		void (*print_func)(const char *s))
{
	LOG(3, NULL);

	util_set_alloc_funcs(malloc_func, free_func,
			realloc_func, strdup_func);
	out_set_print_func(print_func);
	je_cto_pool_set_alloc_funcs(malloc_func, free_func);
}

/*
 * pmemcto_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemcto_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmemcto_errormsg -- return last error message
 */
const char *
pmemcto_errormsg(void)
{
	return pmemcto_errormsgU();
}
#else
/*
 * pmemcto_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmemcto_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif
