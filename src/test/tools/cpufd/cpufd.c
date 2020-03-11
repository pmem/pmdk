// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * cpufd.c -- cpu feature detector - tool to detect
 * the best SIMD extension available.
 *
 * Return values:
 *	1 - SSE2 available
 *	2 - AVX available
 *	3 - AVX512 available
 */

#include "cpu.h"

int
main(int argc, char *argv[])
{
	/* SSE2 by default */
	int ret = 1;

	if (is_cpu_avx_present())
		ret = 2;

	if (is_cpu_avx512f_present())
		ret = 3;

	return ret;
}
