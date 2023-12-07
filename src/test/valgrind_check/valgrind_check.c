// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * valgrind_check.c -- unit test Valgrind enabled during build
 *
 * usage: valgrind_check
 *
 */

#include "valgrind_internal.h"

#ifndef VALGRIND_ENABLED
#error Valgrind tools not properly configured
#endif

int
main(int argc, char *argv[])
{
#if VALGRIND_ENABLED
	return 0;
#else
	return 1;
#endif
}
