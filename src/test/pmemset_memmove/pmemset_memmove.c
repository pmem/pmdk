// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_memmove.c -- test for doing a memmove on pmemeset
 *
 * usage:
 * pmemset_memmove file b:length [d:{offset}] [s:offset] [o:{1|2} S:{overlap}]
 *
 */

#include "unittest.h"
#include "ut_pmem2.h"
#include "file.h"
#include "memmove_common.h"
#include "ut_pmemset_utils.h"

static void
do_memmove_variants(char *dst, char *src, const char *file_name,
	size_t dest_off, size_t src_off, size_t bytes, set_persist_fn sp,
	set_memmove_fn sm, struct pmemset *set)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memmove(dst, src, file_name, dest_off, src_off,
				bytes, NULL, Flags[i], NULL, set, sp, sm);
	}
}

int
main(int argc, char *argv[])
{
	char *dst;
	char *src;
	char *src_orig;
	size_t dst_off = 0;
	size_t src_off = 0;
	size_t bytes = 0;
	int who = 0;
	size_t mapped_len;

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmemset_memmove %s %s %s %s %savx %savx512f",
			argc > 2 ? argv[2] : "null",
			argc > 3 ? argv[3] : "null",
			argc > 4 ? argv[4] : "null",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	if (argc < 3)
		USAGE();

	struct pmemset_part *part;
	struct pmemset_source *ssrc;
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_part_descriptor desc;

	int ret = pmemset_source_from_file(&ssrc, argv[1], 0);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

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

	mapped_len = desc.size;
	dst = desc.addr;

	if (dst == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	for (int arg = 2; arg < argc; arg++) {
		if (strchr("dsbo",
		    argv[arg][0]) == NULL || argv[arg][1] != ':')
			UT_FATAL("op must be d: or s: or b: or o:");

		size_t val = STRTOUL(&argv[arg][2], NULL, 0);

		switch (argv[arg][0]) {
		case 'd':
			if (val <= 0)
				UT_FATAL("bad offset (%lu) with d: option",
						val);
			dst_off = val;
			break;

		case 's':
			if (val <= 0)
				UT_FATAL("bad offset (%lu) with s: option",
						val);
			src_off = val;
			break;

		case 'b':
			if (val <= 0)
				UT_FATAL("bad length (%lu) with b: option",
						val);
			bytes = val;
			break;

		case 'o':
			if (val != 1 && val != 0)
				UT_FATAL("bad val (%lu) with o: option",
						val);
			who = (int)val;
			break;
		}
	}

	if (who == 0) {
		src_orig = src = dst + mapped_len / 2;
		UT_ASSERT(src > dst);

		do_memmove_variants(dst, src, argv[1], dst_off, src_off,
			bytes, pmemset_persist, pmemset_memmove, set);

		/* dest > src */
		src = dst;
		dst = src_orig;

		if (dst <= src)
			UT_FATAL("cannot map files in memory order");

		do_memmove_variants(dst, src, argv[1], dst_off, src_off,
			bytes, pmemset_persist, pmemset_memmove, set);
	} else {
		/* use the same buffer for source and destination */
		memset(dst, 0, bytes);
		pmemset_persist(set, dst, bytes);
		do_memmove_variants(dst, dst, argv[1], dst_off, src_off,
			bytes, pmemset_persist, pmemset_memmove, set);
	}

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&ssrc);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	DONE(NULL);
}
