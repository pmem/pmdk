// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019, Intel Corporation */

/*
 * errormsg.c -- pmem2_errormsg* implementation
 */

#include "libpmem2.h"
#include "out.h"

/*
 * pmem2_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmem2_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmem2_errormsg -- return last error message
 */
const char *
pmem2_errormsg(void)
{
	return pmem2_errormsgU();
}
#else
/*
 * pmem2_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmem2_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif
