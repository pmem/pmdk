/*
 * Copyright 2015-2016, Intel Corporation
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
 * util_cpuid.c -- unit test for CPU features detection
 */

#define _GNU_SOURCE
#include <emmintrin.h>

#include "unittest.h"
#include "cpu.h"

/*
 * The x86 memory instructions are new enough that the compiler
 * intrinsic functions are not always available.  The intrinsic
 * functions are defined here in terms of asm statements for now.
 */
#define _mm_clflushopt(addr)\
	asm volatile(".byte 0x66; clflush %0" : "+m" (*(volatile char *)addr));
#define _mm_clwb(addr)\
	asm volatile(".byte 0x66; xsaveopt %0" : "+m" (*(volatile char *)addr));

static char Buf[32];

/*
 * check_cpu_features -- validates CPU features detection
 */
static void
check_cpu_features(void)
{
	if (is_cpu_sse2_present()) {
		UT_OUT("SSE2 supported");
		char Buf[32];
		__m128i xmm0 = _mm_set1_epi8(0x55);
		/* align to 16B boundary */
		void *dest = (void *)(((uintptr_t)Buf + 16 - 1)
						& ~((uintptr_t)16 - 1));
		UT_OUT("%p %p", Buf, dest);
		_mm_stream_si128(dest, xmm0);
	} else {
		UT_OUT("SSE2 not supported");
	}

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
