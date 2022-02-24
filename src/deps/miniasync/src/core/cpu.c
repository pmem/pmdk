// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

/*
 * cpu.c -- CPU features detection
 */

/*
 * Reference:
 * http://www.intel.com/content/www/us/en/processors/
 * architectures-software-developer-manuals.html
 *
 * https://support.amd.com/TechDocs/24594.pdf
 */

#include <string.h>

#include "cpu.h"

#define EAX_IDX 0
#define EBX_IDX 1
#define ECX_IDX 2
#define EDX_IDX 3

#if defined(__x86_64__) || defined(__amd64__)

#include <cpuid.h>

static inline void
cpuid(unsigned func, unsigned subfunc, unsigned cpuinfo[4])
{
	__cpuid_count(func, subfunc, cpuinfo[EAX_IDX], cpuinfo[EBX_IDX],
			cpuinfo[ECX_IDX], cpuinfo[EDX_IDX]);
}

#elif defined(_M_X64) || defined(_M_AMD64)

#include <intrin.h>

static inline void
cpuid(unsigned func, unsigned subfunc, unsigned cpuinfo[4])
{
	__cpuidex((int *)cpuinfo, (int)func, (int)subfunc);
}

#else

#error unsupported compiler

#endif

#ifndef bit_MOVDIR64B
#define bit_MOVDIR64B (1 << 28)
#endif

/*
 * is_cpu_feature_present -- (internal) checks if CPU feature is supported
 */
static int
is_cpu_feature_present(unsigned func, unsigned reg, unsigned bit)
{
	unsigned cpuinfo[4] = { 0 };

	/* check CPUID level first */
	cpuid(0x0, 0x0, cpuinfo);
	if (cpuinfo[EAX_IDX] < func)
		return 0;

	cpuid(func, 0x0, cpuinfo);
	return (cpuinfo[reg] & bit) != 0;
}

/*
 * is_cpu_movdir64b_present -- checks if movdir64b instruction is supported
 */
int
is_cpu_movdir64b_present(void)
{
	return is_cpu_feature_present(0x7, ECX_IDX, bit_MOVDIR64B);
}
