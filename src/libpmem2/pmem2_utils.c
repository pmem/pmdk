// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * pmem2_utils.c -- libpmem2 utilities functions
 */

#include <errno.h>
#include "alloc.h"
#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"
#include "util.h"

/*
 * pmem2_malloc -- allocate buffer and handle error
 */
void *
pmem2_malloc(size_t size, int *err)
{
	void *ptr = Malloc(size);
	*err = 0;

	if (ptr == NULL) {
		ERR("!malloc(%zu)", size);
		*err = PMEM2_E_ERRNO;
	}

	return ptr;
}

int
pmem2_err_to_errno(int err)
{
	if (err > 0)
		FATAL("positive error code is a bug in libpmem2");

	err = -err;
	if (err < PMEM2_E_UNKNOWN)
		return err;

	return EINVAL;
}

#ifdef _WIN32
/*
 * converts windows error codes to pmem2 error
 */
int
pmem2_lasterror_to_err()
{
	int err = util_lasterror_to_errno(GetLastError());

	if (err == -1)
		return PMEM2_E_UNKNOWN;

	return -err;
}
#endif
