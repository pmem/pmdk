// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_movnt_align.c -- test for functions with non-temporal stores
 *
 * usage: pmem2_movnt_align file [C|F|B|S]
 *
 * C - pmem2_memcpy()
 * B - pmem2_memmove() in backward direction
 * F - pmem2_memmove() in forward direction
 * S - pmem2_memset()
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libpmem2.h"
#include "unittest.h"
#include "movnt_align_common.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"

static pmem2_memset_fn memset_fn;
static pmem2_memcpy_fn memcpy_fn;
static pmem2_memmove_fn memmove_fn;

static void
check_memmove_variants(size_t doff, size_t soff, size_t len)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memmove(doff, soff, len, memmove_fn, Flags[i]);
}

static void
check_memcpy_variants(size_t doff, size_t soff, size_t len)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memcpy(doff, soff, len, memcpy_fn, Flags[i]);
}

static void
check_memset_variants(size_t off, size_t len)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i)
		check_memset(off, len, memset_fn, Flags[i]);
}

int
main(int argc, char *argv[])
{
	if (argc != 3)
		UT_FATAL("usage: %s file type", argv[0]);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	struct pmem2_map *map;
	int fd;

	char type = argv[2][0];
	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_movnt_align %c %s %savx %savx512f", type,
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_SOURCE_FROM_FD(&src, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);

	memset_fn = pmem2_get_memset_fn(map);
	memcpy_fn = pmem2_get_memcpy_fn(map);
	memmove_fn = pmem2_get_memmove_fn(map);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	size_t page_size = Ut_pagesize;
	size_t s;
	switch (type) {
	case 'C': /* memcpy */
		/* mmap with guard pages */
		Src = MMAP_ANON_ALIGNED(N_BYTES, 0);
		Dst = MMAP_ANON_ALIGNED(N_BYTES, 0);
		if (Src == NULL || Dst == NULL)
			UT_FATAL("!mmap");

		Scratch = MALLOC(N_BYTES);

		/* check memcpy with 0 size */
		check_memcpy_variants(0, 0, 0);

		/* check memcpy with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(0, 0, N_BYTES - s);

		/* check memcpy with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(s, 0, N_BYTES - s);

		/* check memcpy with unaligned begin and end */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memcpy_variants(s, s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Src, N_BYTES);
		MUNMAP_ANON_ALIGNED(Dst, N_BYTES);
		FREE(Scratch);

		break;
	case 'B': /* memmove backward */
		/* mmap with guard pages */
		Src = MMAP_ANON_ALIGNED(2 * N_BYTES - page_size, 0);
		Dst = Src + N_BYTES - page_size;
		if (Src == NULL)
			UT_FATAL("!mmap");

		/* check memmove in backward direction with 0 size */
		check_memmove_variants(0, 0, 0);

		/* check memmove in backward direction with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(0, 0, N_BYTES - s);

		/* check memmove in backward direction with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, 0, N_BYTES - s);

		/*
		 * check memmove in backward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Src, 2 * N_BYTES - page_size);
		break;
	case 'F': /* memmove forward */
		/* mmap with guard pages */
		Dst = MMAP_ANON_ALIGNED(2 * N_BYTES - page_size, 0);
		Src = Dst + N_BYTES - page_size;
		if (Src == NULL)
			UT_FATAL("!mmap");

		/* check memmove in forward direction with 0 size */
		check_memmove_variants(0, 0, 0);

		/* check memmove in forward direction with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(0, 0, N_BYTES - s);

		/* check memmove in forward direction with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, 0, N_BYTES - s);

		/*
		 * check memmove in forward direction with unaligned begin
		 * and end
		 */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memmove_variants(s, s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Dst, 2 * N_BYTES - page_size);

		break;
	case 'S': /* memset */
		/* mmap with guard pages */
		Dst = MMAP_ANON_ALIGNED(N_BYTES, 0);
		if (Dst == NULL)
			UT_FATAL("!mmap");

		Scratch = MALLOC(N_BYTES);

		/* check memset with 0 size */
		check_memset_variants(0, 0);

		/* check memset with unaligned size */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(0, N_BYTES - s);

		/* check memset with unaligned begin */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(s, N_BYTES - s);

		/* check memset with unaligned begin and end */
		for (s = 0; s < CACHELINE_SIZE; s++)
			check_memset_variants(s, N_BYTES - 2 * s);

		MUNMAP_ANON_ALIGNED(Dst, N_BYTES);
		FREE(Scratch);

		break;
	default:
		UT_FATAL("!wrong type of test");
		break;
	}

	DONE(NULL);
}
