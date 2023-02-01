// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * util_cpuid.c -- unit test for CPU features detection
 */

#define _GNU_SOURCE
#include <emmintrin.h>

#include "unittest.h"
#include "cpu.h"

#define _mm_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" :\
	"+m" (*(volatile char *)(addr)));
#define _mm_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" :\
	"+m" (*(volatile char *)(addr)));

static char Buf[32];

/*
 * check_cpu_features -- validates CPU features detection
 */
static void
check_cpu_features(void)
{
	if (is_cpu_clflush_present()) {
		UT_OUT("CLFLUSH supported");
		_mm_clflush(Buf);
	} else {
		UT_OUT("CLFLUSH not supported");
	}

	if (is_cpu_clflushopt_present()) {
		UT_OUT("CLFLUSHOPT supported");
		_mm_clflushopt(Buf);
	} else {
		UT_OUT("CLFLUSHOPT not supported");
	}

	if (is_cpu_clwb_present()) {
		UT_OUT("CLWB supported");
		_mm_clwb(Buf);
	} else {
		UT_OUT("CLWB not supported");
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "util_cpuid");

	check_cpu_features();

	DONE(NULL);
}
