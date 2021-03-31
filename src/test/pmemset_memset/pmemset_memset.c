// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_memset.c -- test for doing a memset from libpmemset
 *
 * usage: pmemset_memset file offset length
 *
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmemset_utils.h"
#include "memset_common.h"

static void
do_memset_variants(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, set_persist_fn sp, set_memset_fn sm,
		struct pmemset *set)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memset(fd, dest, file_name, dest_off, bytes,
				NULL, Flags[i], NULL, sp, sm, set);
		if (Flags[i] & PMEMOBJ_F_MEM_NOFLUSH)
			sp(set, dest, bytes);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	int ret;
	char *dest;
	struct pmem2_source *pmem2_src;
	struct pmemset_source *ssrc;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;
	struct pmemset_part_descriptor desc;

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

	ret = pmem2_source_from_fd(&pmem2_src, fd);
	UT_ASSERTeq(ret, 0);

	ret = pmemset_source_from_pmem2(&ssrc, pmem2_src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(ssrc, NULL);

	ret = pmemset_config_new(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_config_set_required_store_granularity(cfg,
		PMEM2_GRANULARITY_PAGE);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(cfg, NULL);

	ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ut_create_map_config(&map_cfg, set, 0, 4 * 1024 * 1024);

	ret = pmemset_map(ssrc, map_cfg, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	dest = desc.addr;

	size_t dest_off = strtoul(argv[2], NULL, 0);
	size_t bytes = strtoul(argv[3], NULL, 0);

	do_memset_variants(fd, dest, argv[1], dest_off, bytes,
		pmemset_persist, pmemset_memset, set);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_map_config_delete(&map_cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&ssrc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}
