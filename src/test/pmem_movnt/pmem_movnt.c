/*
 * Copyright 2015-2019, Intel Corporation
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
 * pmem_movnt.c -- unit test for MOVNT threshold
 *
 * usage: pmem_movnt
 *
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	char *dst;
	char *src;

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem_movnt %s %savx %savx512f",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	src = MEMALIGN(64, 8192);
	dst = MEMALIGN(64, 8192);

	memset(src, 0x88, 8192);
	memset(dst, 0, 8192);

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		pmem_memcpy_nodrain(dst, src, size);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		pmem_memmove_nodrain(dst, src, size);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		pmem_memset_nodrain(dst, 0x77, size);
		UT_ASSERTeq(dst[0], 0x77);
		UT_ASSERTeq(dst[size - 1], 0x77);
		UT_ASSERTeq(dst[size], 0);
	}

	ALIGNED_FREE(dst);
	ALIGNED_FREE(src);

	DONE(NULL);
}
