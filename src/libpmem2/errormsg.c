// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2023, Intel Corporation */

/*
 * errormsg.c -- pmem2_errormsg* implementation
 */

#include "libpmem2.h"
#include "out.h"
#include "pmem2_utils.h"

/*
 * pmem2_errormsgU -- return last error message
 */
static inline
const char *
pmem2_errormsgU(void)
{
	return out_get_errormsg();
}

/*
 * pmem2_errormsg -- return last error message
 */
const char *
pmem2_errormsg(void)
{
	return pmem2_errormsgU();
}

/*
 * pmem2_perrorU -- prints a descriptive error message to the stderr
 */
static inline void
pmem2_perrorU(const char *format, va_list args)
{
	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", pmem2_errormsg());
}

/*
 * pmem2_perror -- prints a descriptive error message to the stderr
 */
void
pmem2_perror(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	pmem2_perrorU(format, args);

	va_end(args);
}
