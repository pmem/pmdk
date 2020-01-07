// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

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
