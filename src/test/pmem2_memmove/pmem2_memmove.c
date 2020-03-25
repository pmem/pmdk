// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * pmem2_memmove.c -- test for doing a memmove
 *
 * usage:
 * pmem2_memmove file b:length [d:{offset}] [s:offset] [o:{1|2} S:{overlap}]
 *
 */

#include "unittest.h"
#include "ut_pmem2.h"
#include "file.h"
#include "memmove_common.h"

static void
do_memmove_variants(char *dst, char *src, const char *file_name,
	size_t dest_off, size_t src_off, size_t bytes, persist_fn p,
	memmove_fn fn)
{
	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memmove(dst, src, file_name, dest_off, src_off,
				bytes, fn, Flags[i], p);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dst;
	char *src;
	char *src_orig;
	size_t dst_off = 0;
	size_t src_off = 0;
	size_t bytes = 0;
	int who = 0;
	size_t mapped_len;
	struct pmem2_config *cfg;
	struct pmem2_source *psrc;
	struct pmem2_map *map;

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem2_memmove %s %s %s %s %savx %savx512f",
			argc > 2 ? argv[2] : "null",
			argc > 3 ? argv[3] : "null",
			argc > 4 ? argv[4] : "null",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);

	if (argc < 3)
		USAGE();

	PMEM2_CONFIG_NEW(&cfg);
	PMEM2_SOURCE_FROM_FD(&psrc, fd);
	PMEM2_CONFIG_SET_GRANULARITY(cfg, PMEM2_GRANULARITY_PAGE);

	int ret = pmem2_map(cfg, psrc, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_CONFIG_DELETE(&cfg);

	pmem2_persist_fn persist = pmem2_get_persist_fn(map);

	mapped_len = pmem2_map_get_size(map);
	dst = pmem2_map_get_address(map);
	if (dst == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);

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
			bytes, persist, memmove_fn);

		/* dest > src */
		src = dst;
		dst = src_orig;

		if (dst <= src)
			UT_FATAL("cannot map files in memory order");

		do_memmove_variants(dst, src, argv[1], dst_off, src_off,
			bytes, persist, memmove_fn);
	} else {
		/* use the same buffer for source and destination */
		memset(dst, 0, bytes);
		persist(dst, bytes);
		do_memmove_variants(dst, dst, argv[1], dst_off, src_off,
			bytes, persist, memmove_fn);
	}

	ret = pmem2_unmap(&map);
	UT_ASSERTeq(ret, 0);

	CLOSE(fd);

	DONE(NULL);
}
