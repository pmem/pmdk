// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

#include "libpmemset.h"

#include "out.h"

#ifndef _WIN32
/*
 * pmemset_errormsg -- return last error message
 */
inline const char *
pmemset_errormsg(void)
{
	return out_get_errormsg();
}

/*
 * pmemset_perror -- prints a descriptive error message to the stderr
 */
inline void
pmemset_perror(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", pmemset_errormsg());

	va_end(args);
}
#else
/*
 * pmemset_errormsgU -- return last error message
 */
const char *
pmemset_errormsgU(void)
{
	return out_get_errormsg();
}

/*
 * pmemset_errormsgW -- return last error as wchat_t
 */
const wchar_t *
pmemset_errormsgW(void)
{
	return out_get_errormsgW();
}

/*
 * pmemset_perrorU -- prints a descriptive error message to the stderr
 */
void
pmemset_perrorU(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	vfprintf(stderr, format, args);
	fprintf(stderr, ": %s\n", pmemset_errormsgU());

	va_end(args);
}

/*
 * pmemset_perrorW -- prints a descriptive error message to the stderr
 */
void
pmemset_perrorW(const wchar_t *format, ...)
{
	va_list args;
	va_start(args, format);

	vfwprintf(stderr, format, args);
	fwprintf(stderr, L": %s\n", pmemset_errormsgW());

	va_end(args);
}
#endif
