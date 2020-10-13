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
