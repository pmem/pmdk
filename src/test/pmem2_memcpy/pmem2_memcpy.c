// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_memcpy.c -- test for doing a memcpy from libpmem2
 *
 * usage: pmem2_memcpy file destoff srcoff length
 *
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#include "memcpy_common.h"

/*
 * do_memcpy_variants -- do_memcpy wrapper that tests multiple variants
 * of memcpy functions
 */
static void
do_memcpy_variants(int fd, char *dest, int dest_off, char *src, int src_off,
		    size_t bytes, size_t mapped_len, const char *file_name,
		    persist_fn p, memcpy_fn fn)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memcpy(fd, dest, dest_off, src, src_off, bytes, mapped_len,
			file_name, fn, Flags[i], p);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	char *src;
	char *src_orig;
	size_t mapped_len;
	struct pmem2_config *cfg;
	struct pmem2_source *psrc;
	struct pmem2_map *map;

	if (argc != 5)
		UT_FATAL("usage: %s file destoff srcoff length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_memcpy %s %s %s %s %savx %savx512f",
			argv[2], argv[3], argv[4], thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);
	UT_ASSERT(fd != -1);
	int dest_off = atoi(argv[2]);
	int src_off = atoi(argv[3]);
	size_t bytes = strtoul(argv[4], NULL, 0);

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_SOURCE_FROM_FD(&psrc, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(cfg, psrc, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);

	/* src > dst */
	mapped_len = pmem2_map_get_size(map);
	dest = pmem2_map_get_address(map);
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	src_orig = src = dest + mapped_len / 2;
	UT_ASSERT(src > dest);

	pmem2_persist_fn persist = pmem2_get_persist_fn(map);
	memset(dest, 0, (2 * bytes));
	persist(dest, 2 * bytes);
	memset(src, 0, (2 * bytes));

	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes,
		0, argv[1], persist, memcpy_fn);

	src = dest;
	dest = src_orig;

	if (dest <= src)
		UT_FATAL("cannot map files in memory order");

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes, mapped_len,
		argv[1], persist, memcpy_fn);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}
