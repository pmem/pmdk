// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmemset_utils.c -- libpmemset utilities functions
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
