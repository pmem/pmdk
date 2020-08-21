/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * err.h - error and warning messages
 */

#ifndef ERR_H
#define ERR_H 1

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

/*
 * err - windows implementation of unix err function
 */
__declspec(noreturn) static void
err(int eval, const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	vfprintf(stderr, fmt, vl);
	va_end(vl);
	exit(eval);
}

/*
 * warn - windows implementation of unix warn function
 */
static void
warn(const char *fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	fprintf(stderr, "Warning: ");
	vfprintf(stderr, fmt, vl);
	va_end(vl);
}

#endif /* ERR_H */
