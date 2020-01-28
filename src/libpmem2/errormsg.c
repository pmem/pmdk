// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

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

/*
 * pmem2_perrorU -- prints a descriptive error message to the stderr
 */
#ifndef _WIN32
static inline void
pmem2_perrorU(const char *format, va_list args)
{
	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", pmem2_errormsg());
}
#else
void
pmem2_perrorU(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", pmem2_errormsg());

	va_end(args);
}
#endif

#ifndef _WIN32
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
#else
/*
 * pmem2_perrorW -- prints a descriptive error message to the stderr
 */
void
pmem2_perrorW(const wchar_t *format, ...)
{
	va_list args;
	va_start(args, format);

	vfwprintf(stderr, format, args);
	fwprintf(stderr, L": %s\n", pmem2_errormsgW());

	va_end(args);
}
#endif
