// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2017, Intel Corporation */

/*
 * libpmemobj.c -- pmem entry points for libpmemobj
 */

#include "pmemcommon.h"
#include "obj.h"

/*
 * libpmemobj_init -- load-time initialization for obj
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmemobj_init(void)
{
	common_init(PMEMOBJ_LOG_PREFIX, PMEMOBJ_LOG_LEVEL_VAR,
			PMEMOBJ_LOG_FILE_VAR, PMEMOBJ_MAJOR_VERSION,
			PMEMOBJ_MINOR_VERSION);
	LOG(3, NULL);
	obj_init();
}

/*
 * libpmemobj_fini -- libpmemobj cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmemobj_fini(void)
{
	LOG(3, NULL);
	obj_fini();
	common_fini();
}

/*
 * pmemobj_check_versionU -- see if lib meets application version requirements
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemobj_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEMOBJ_MAJOR_VERSION) {
		ERR("libpmemobj major version mismatch (need %u, found %u)",
			major_required, PMEMOBJ_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > PMEMOBJ_MINOR_VERSION) {
		ERR("libpmemobj minor version mismatch (need %u, found %u)",
			minor_required, PMEMOBJ_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

#ifndef _WIN32
/*
 * pmemobj_check_version -- see if lib meets application version requirements
 */
const char *
pmemobj_check_version(unsigned major_required, unsigned minor_required)
{
	return pmemobj_check_versionU(major_required, minor_required);
}
#else
/*
 * pmemobj_check_versionW -- see if lib meets application version requirements
 */
const wchar_t *
pmemobj_check_versionW(unsigned major_required, unsigned minor_required)
{
	if (pmemobj_check_versionU(major_required, minor_required) != NULL)
		return out_get_errormsgW();
	else
		return NULL;
}
#endif

/*
 * pmemobj_set_funcs -- allow overriding libpmemobj's call to malloc, etc.
 */
void
pmemobj_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s))
{
	LOG(3, NULL);

	util_set_alloc_funcs(malloc_func, free_func, realloc_func, strdup_func);
}

/*
 * pmemobj_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemobj_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmemobj_errormsg -- return last error message
 */
const char *
pmemobj_errormsg(void)
{
	return pmemobj_errormsgU();
}
#else
/*
 * pmemobj_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmemobj_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif
