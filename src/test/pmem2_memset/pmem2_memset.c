// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem_memset.c -- unit test for doing a memset
 *
 * usage: pmem_memset file offset length
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmem2.h"
#include "memset_common.h"

static void
do_memset_variants(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, persist_fn p, memset_fn fn)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memset(fd, dest, file_name, dest_off, bytes,
				fn, Flags[i], p);
		if (Flags[i] & PMEMOBJ_F_MEM_NOFLUSH)
			p(dest, bytes);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	struct pmem2_config *cfg;
	struct pmem2_source *src;
	struct pmem2_map *map;

	if (argc != 4)
		UT_FATAL("usage: %s file offset length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_memset %s %s %s %savx %savx512f",
			argv[2], argv[3],
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

	dest = pmem2_map_get_address(map);
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	size_t dest_off = strtoul(argv[2], NULL, 0);
	size_t bytes = strtoul(argv[3], NULL, 0);

	pmem2_persist_fn persist = pmem2_get_persist_fn(map);

	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);
	do_memset_variants(fd, dest, argv[1], dest_off, bytes,
		persist, memset_fn);

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}
