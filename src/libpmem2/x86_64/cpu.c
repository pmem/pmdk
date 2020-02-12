/*
 * Copyright 2015-2020, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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

#include "out.h"
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
	__cpuidex(cpuinfo, func, subfunc);
}

#else

#error unsupported compiler

#endif

#ifndef bit_CLFLUSH
#define bit_CLFLUSH	(1 << 19)
#endif

#ifndef bit_CLFLUSHOPT
#define bit_CLFLUSHOPT	(1 << 23)
#endif

#ifndef bit_CLWB
#define bit_CLWB	(1 << 24)
#endif

#ifndef bit_AVX
#define bit_AVX		(1 << 28)
#endif

#ifndef bit_AVX512F
#define bit_AVX512F	(1 << 16)
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
 * is_cpu_clflush_present -- checks if CLFLUSH instruction is supported
 */
int
is_cpu_clflush_present(void)
{
	int ret = is_cpu_feature_present(0x1, EDX_IDX, bit_CLFLUSH);
	LOG(4, "CLFLUSH %ssupported", ret == 0 ? "not " : "");

	return ret;
}

/*
 * is_cpu_clflushopt_present -- checks if CLFLUSHOPT instruction is supported
 */
int
is_cpu_clflushopt_present(void)
{
	int ret = is_cpu_feature_present(0x7, EBX_IDX, bit_CLFLUSHOPT);
	LOG(4, "CLFLUSHOPT %ssupported", ret == 0 ? "not " : "");

	return ret;
}

/*
 * is_cpu_clwb_present -- checks if CLWB instruction is supported
 */
int
is_cpu_clwb_present(void)
{
	int ret = is_cpu_feature_present(0x7, EBX_IDX, bit_CLWB);
	LOG(4, "CLWB %ssupported", ret == 0 ? "not " : "");

	return ret;
}

/*
 * is_cpu_avx_present -- checks if AVX instructions are supported
 */
int
is_cpu_avx_present(void)
{
	int ret = is_cpu_feature_present(0x1, ECX_IDX, bit_AVX);
	LOG(4, "AVX %ssupported", ret == 0 ? "not " : "");

	return ret;
}

/*
 * is_cpu_avx512f_present -- checks if AVX-512f instructions are supported
 */
int
is_cpu_avx512f_present(void)
{
	int ret = is_cpu_feature_present(0x7, EBX_IDX, bit_AVX512F);
	LOG(4, "AVX512f %ssupported", ret == 0 ? "not " : "");

	return ret;
}
