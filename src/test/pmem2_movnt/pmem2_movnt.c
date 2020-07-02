// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_movnt.c -- test for MOVNT threshold
 *
 * usage: pmem2_movnt
 */

#include "unittest.h"
#include "ut_pmem2.h"

int
main(int argc, char *argv[])
{
	int fd;
	char *dst;
	char *src;
	struct pmem2_config *cfg;
	struct pmem2_source *psrc;
	struct pmem2_map *map;

	if (argc != 2)
		UT_FATAL("usage: %s file", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_movnt %s %savx %savx512f",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_SOURCE_FROM_FD(&psrc, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(&map, cfg, psrc);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);

	src = MEMALIGN(64, 8192);
	dst = MEMALIGN(64, 8192);

	memset(src, 0x88, 8192);
	memset(dst, 0, 8192);

	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		memcpy_fn(dst, src, size, PMEM2_F_MEM_NODRAIN);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		memmove_fn(dst, src, size, PMEM2_F_MEM_NODRAIN);
		UT_ASSERTeq(memcmp(src, dst, size), 0);
		UT_ASSERTeq(dst[size], 0);
	}

	for (size_t size = 1; size <= 4096; size *= 2) {
		memset(dst, 0, 4096);
		memset_fn(dst, 0x77, size, PMEM2_F_MEM_NODRAIN);
		UT_ASSERTeq(dst[0], 0x77);
		UT_ASSERTeq(dst[size - 1], 0x77);
		UT_ASSERTeq(dst[size], 0);
	}

	ALIGNED_FREE(dst);
	ALIGNED_FREE(src);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}
