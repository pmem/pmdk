// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2023, Intel Corporation */

/*
 * valgrind_check.c -- unit test Valgrind enabled during build
 *
 * usage: valgrind_check
 *
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
#ifndef VALGRIND_ENABLED
	return 0;
#else
#if VALGRIND_ENABLED
	return 0;
#else
	return 1;
#endif
#endif
}