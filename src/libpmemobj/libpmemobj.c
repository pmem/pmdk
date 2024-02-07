// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2024, Intel Corporation */

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
static inline
const char *
pmemobj_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEMOBJ_MAJOR_VERSION) {
		ERR_WO_ERRNO(
			"libpmemobj major version mismatch (need %u, found %u)",
			major_required, PMEMOBJ_MAJOR_VERSION);
		return last_error_msg_get();
	}

	if (minor_required > PMEMOBJ_MINOR_VERSION) {
		ERR_WO_ERRNO(
			"libpmemobj minor version mismatch (need %u, found %u)",
			minor_required, PMEMOBJ_MINOR_VERSION);
		return last_error_msg_get();
	}

	return NULL;
}

/*
 * pmemobj_check_version -- see if lib meets application version requirements
 */
const char *
pmemobj_check_version(unsigned major_required, unsigned minor_required)
{
	return pmemobj_check_versionU(major_required, minor_required);
}

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
 * pmemobj_errormsgU -- return the last error message
 */
static inline
const char *
pmemobj_errormsgU(void)
{
	return last_error_msg_get();
}

/*
 * pmemobj_errormsg -- return the last error message
 */
const char *
pmemobj_errormsg(void)
{
	return pmemobj_errormsgU();
}
