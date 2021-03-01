// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_memcpy.c -- test for doing a memcpy from libpmemset
 *
 * usage: pmemset_memcpy file destoff srcoff length
 *
 */

#include "unittest.h"
#include "file.h"
#include "ut_pmem2.h"
#include "memcpy_common.h"
#include "ut_pmemset_utils.h"

/*
 * do_memcpy_variants -- do_memcpy wrapper that tests multiple variants
 * of memcpy functions
 */
static void
do_memcpy_variants(int fd, char *dest, int dest_off, char *src, int src_off,
		    size_t bytes, size_t mapped_len, const char *file_name,
		    set_persist_fn sp, set_memcpy_fn sm, struct pmemset *set)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memcpy(fd, dest, dest_off, src, src_off, bytes, mapped_len,
			file_name, NULL, Flags[i], NULL, set, sp, sm);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	int ret;
	char *dest;
	char *src;
	char *src_orig;
	size_t mapped_len;

	if (argc != 5)
		UT_FATAL("usage: %s file destoff srcoff length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmemset_memcpy %s %s %s %s %savx %savx512f",
			argv[2], argv[3], argv[4], thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");
	util_init();

	struct pmem2_source *pmem2_src;
	struct pmemset_part *part;
	struct pmemset_source *ssrc;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc;

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

	ret = pmemset_part_new(&part, set, ssrc, 0, 4 * 1024 * 1024);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_part_map(&part, NULL, &desc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	UT_ASSERTeq(part, NULL);

	/* src > dst */
	mapped_len = desc.size;
	dest = desc.addr;

	int dest_off = atoi(argv[2]);
	int src_off = atoi(argv[3]);
	size_t bytes = strtoul(argv[4], NULL, 0);

	src_orig = src = dest + mapped_len / 2;
	UT_ASSERT(src > dest);

	memset(dest, 0, (2 * bytes));
	pmemset_persist(set, dest, 2 * bytes);
	memset(src, 0, (2 * bytes));
	pmemset_persist(set, src, 2 * bytes);

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes,
		0, argv[1], pmemset_persist, pmemset_memcpy, set);

	src = dest;
	dest = src_orig;

	if (dest <= src)
		UT_FATAL("cannot map files in memory order");

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes, mapped_len,
		argv[1], pmemset_persist, pmemset_memcpy, set);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&ssrc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}
