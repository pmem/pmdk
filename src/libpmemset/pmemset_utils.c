// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_utils.c -- libpmemset utility functions
 */

#include <errno.h>

#include "alloc.h"
#include "libpmemset.h"
#include "out.h"
#include "pmemset_utils.h"
#include "util.h"

/*
 * pmemset_malloc -- allocate a buffer and handle an error
 */
void *
pmemset_malloc(size_t size, int *err)
{
	void *ptr = Malloc(size);
	*err = 0;

	if (ptr == NULL) {
		ERR("!malloc(%zu)", size);
		*err = PMEMSET_E_ERRNO;
	}

	return ptr;
}

/*
 * pmemset_zalloc -- allocate a buffer, zero it and handle an error
 */
void *
pmemset_zalloc(size_t size, int *err)
{
	void *ptr = Zalloc(size);
	*err = 0;

	if (ptr == NULL) {
		ERR("!malloc(%zu)", size);
		*err = PMEMSET_E_ERRNO;
	}

	return ptr;
}

/*
 * pmemset_realloc -- reallocate a buffer and handle an error
 */
void *
pmemset_realloc(void *ptr, size_t size, int *err)
{
	void *newptr = Realloc(ptr, size);
	*err = 0;

	if (newptr == NULL) {
		ERR("!realloc(%zu)", size);
		*err = PMEMSET_E_ERRNO;
	}

	return newptr;
}

int
pmemset_err_to_errno(int err)
{
	if (err > 0)
		FATAL("positive error code is a bug in libpmemset");

	if (err == PMEMSET_E_NOSUPP)
		return ENOTSUP;

	if (err <= PMEMSET_E_UNKNOWN)
		return EINVAL;

	return -err;
}

#ifdef _WIN32
/*
 * converts windows error codes to pmemset error
 */
int
pmemset_lasterror_to_err()
{
	int err = util_lasterror_to_errno(GetLastError());

	if (err == -1)
		return PMEMSET_E_UNKNOWN;

	return -err;
}
#endif
