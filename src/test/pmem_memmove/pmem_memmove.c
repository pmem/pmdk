// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * pmem_memmove.c -- unit test for doing a memmove
 *
 * usage:
 * pmem_memmove file b:length [d:{offset}] [s:offset] [o:{1|2} S:{overlap}]
 *
 */

#include "unittest.h"
#include "util_pmem.h"
#include "file.h"
#include "memmove_common.h"

typedef void *pmem_memmove_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);

static void *
pmem_memmove_persist_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memmove_persist(pmemdest, src, len);
}

static void *
pmem_memmove_nodrain_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memmove_nodrain(pmemdest, src, len);
}

static void
do_persist_ddax(const void *ptr, size_t size)
{
	util_persist_auto(1, ptr, size);
}

static void
do_persist(const void *ptr, size_t size)
{
	util_persist_auto(0, ptr, size);
}

/*
 * swap_mappings - given to mmapped regions swap them.
 *
 * Try swapping src and dest by unmapping src, mapping a new dest with
 * the original src address as a hint. If successful, unmap original dest.
 * Map a new src with the original dest as a hint.
 * In the event of an error caller must unmap all passed in mappings.
 */
static void
swap_mappings(char **dest, char **src, size_t size, int fd)
{
	char *d = *dest;
	char *s = *src;
	char *ts;
	char *td;

	MUNMAP(*src, size);

	/* mmap destination using src addr as hint */
	td = MMAP(s, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	MUNMAP(*dest, size);
	*dest = td;

	/* mmap src using original destination addr as a hint */
	ts = MMAP(d, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
		-1, 0);
	*src = ts;
}

static void
do_memmove_variants(char *dst, char *src, const char *file_name,
	size_t dest_off, size_t src_off, size_t bytes, persist_fn p)
{
	do_memmove(dst, src, file_name, dest_off, src_off,
			bytes, pmem_memmove_persist_wrapper, 0, p);
	do_memmove(dst, src, file_name, dest_off, src_off,
			bytes, pmem_memmove_nodrain_wrapper, 0, p);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memmove(dst, src, file_name, dest_off, src_off,
			bytes, pmem_memmove, Flags[i], p);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dst;
	char *src;
	size_t dst_off = 0;
	size_t src_off = 0;
	size_t bytes = 0;
	int who = 0;
	size_t mapped_len;

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem_memmove %s %s %s %s %savx %savx512f",
			argc > 2 ? argv[2] : "null",
			argc > 3 ? argv[3] : "null",
			argc > 4 ? argv[4] : "null",
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);

	enum file_type type = util_fd_get_type(fd);
	if (type < 0)
		UT_FATAL("cannot check type of file %s", argv[1]);

	persist_fn p;
	p = type == TYPE_DEVDAX ? do_persist_ddax : do_persist;

	if (argc < 3)
		USAGE();

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
		/* src > dest */
		dst = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
		if (dst == NULL)
			UT_FATAL("!could not mmap dest file %s", argv[1]);

		src = MMAP(dst + mapped_len, mapped_len,
			PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS,
			-1, 0);
		/*
		 * Its very unlikely that src would not be > dest. pmem_map_file
		 * chooses the first unused address >= 1TB, large
		 * enough to hold the give range, and 1GB aligned. Log
		 * the error if the mapped addresses cannot be swapped
		 * but allow the test to continue.
		 */
		if (src <= dst) {
			swap_mappings(&dst, &src, mapped_len, fd);
			if (src <= dst)
				UT_FATAL("cannot map files in memory order");
		}

		do_memmove_variants(dst, src, argv[1],
				dst_off, src_off, bytes, p);

		/* dest > src */
		swap_mappings(&dst, &src, mapped_len, fd);

		if (dst <= src)
			UT_FATAL("cannot map files in memory order");

		do_memmove_variants(dst, src, argv[1],
				dst_off, src_off, bytes, p);

		int ret = pmem_unmap(dst, mapped_len);
		UT_ASSERTeq(ret, 0);

		MUNMAP(src, mapped_len);
	} else {
		/* use the same buffer for source and destination */
		dst = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
		if (dst == NULL)
			UT_FATAL("!Could not mmap %s: \n", argv[1]);

		memset(dst, 0, bytes);
		p(dst, bytes);
		do_memmove_variants(dst, dst, argv[1],
				dst_off, src_off, bytes, p);

		int ret = pmem_unmap(dst, mapped_len);
		UT_ASSERTeq(ret, 0);
	}

	CLOSE(fd);

	DONE(NULL);
}
