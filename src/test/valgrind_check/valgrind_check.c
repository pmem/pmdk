// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2024, Intel Corporation */

/*
 * valgrind_check.c -- unit test Valgrind enabled during build
 *
 * usage: valgrind_check
 *
 */

int
main(int argc, char *argv[])
{
#ifndef VALGRIND_ENABLED
	return 1;
#else
#if VALGRIND_ENABLED
	return 0;
#else
	return 1;
#endif
#endif
}
