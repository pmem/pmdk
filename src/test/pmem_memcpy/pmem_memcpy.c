// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2021, Intel Corporation */

/*
 * pmem_memcpy.c -- unit test for doing a memcpy
 *
 * usage: pmem_memcpy file destoff srcoff length
 *
 */

#include "unittest.h"
#include "util_pmem.h"
#include "file.h"
#include "memcpy_common.h"

static void *
pmem_memcpy_persist_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memcpy_persist(pmemdest, src, len);
}

static void *
pmem_memcpy_nodrain_wrapper(void *pmemdest, const void *src, size_t len,
		unsigned flags)
{
	(void) flags;
	return pmem_memcpy_nodrain(pmemdest, src, len);
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
 * swap_mappings - swap given two mapped regions.
 *
 * Try swapping src and dest by unmapping src, mapping a new dest with
 * the original src address as a hint. If successful, unmap original dest.
 * Map a new src with the original dest as a hint.
 */
static void
swap_mappings(char **dest, char **src, size_t size, int fd)
{
	char *d = *dest;
	char *s = *src;
	char *td, *ts;

	MUNMAP(*src, size);

	/* mmap destination using src addr as a hint */
	td = MMAP(s, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	MUNMAP(*dest, size);
	*dest = td;

	/* mmap src using original destination addr as a hint */
	ts = MMAP(d, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,
		-1, 0);
	*src = ts;
}

/*
 * do_memcpy_variants -- do_memcpy wrapper that tests multiple variants
 * of memcpy functions
 */
static void
do_memcpy_variants(int fd, char *dest, int dest_off, char *src, int src_off,
		    size_t bytes, size_t mapped_len, const char *file_name,
		    persist_fn p)
{
	do_memcpy(fd, dest, dest_off, src, src_off, bytes, mapped_len,
			file_name, pmem_memcpy_persist_wrapper, 0, p,
			NULL, NULL, NULL);

	do_memcpy(fd, dest, dest_off, src, src_off, bytes, mapped_len,
			file_name, pmem_memcpy_nodrain_wrapper, 0, p,
			NULL, NULL, NULL);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memcpy(fd, dest, dest_off, src, src_off, bytes, mapped_len,
			file_name, pmem_memcpy, Flags[i], p,
			NULL, NULL, NULL);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	char *dest;
	char *src;
	char *dest_orig;
	char *src_orig;
	size_t mapped_len;

	if (argc != 5)
		UT_FATAL("usage: %s file srcoff destoff length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem_memcpy %s %s %s %s %savx %savx512f",
			argv[2], argv[3], argv[4], thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);
	int dest_off = atoi(argv[2]);
	int src_off = atoi(argv[3]);
	size_t bytes = strtoul(argv[4], NULL, 0);

	/* src > dst */
	dest_orig = dest = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
	if (dest == NULL)
		UT_FATAL("!could not map file: %s", argv[1]);

	src_orig = src = MMAP(dest + mapped_len, mapped_len,
			PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS, -1, 0);
	/*
	 * Its very unlikely that src would not be > dest. pmem_map_file
	 * chooses the first unused address >= 1TB, large
	 * enough to hold the give range, and 1GB aligned. If the
	 * addresses did not get swapped to allow src > dst, log error
	 * and allow test to continue.
	 */
	if (src <= dest) {
		swap_mappings(&dest, &src, mapped_len, fd);
		if (src <= dest)
			UT_FATAL("cannot map files in memory order");
	}

	enum file_type type = util_fd_get_type(fd);
	if (type < 0)
		UT_FATAL("cannot check type of file with fd %d", fd);

	persist_fn persist;
	persist = type == TYPE_DEVDAX ? do_persist_ddax : do_persist;
	memset(dest, 0, (2 * bytes));
	persist(dest, 2 * bytes);
	memset(src, 0, (2 * bytes));

	do_memcpy_variants(fd, dest, dest_off, src, src_off,
		bytes, 0, argv[1], persist);

	/* dest > src */
	swap_mappings(&dest, &src, mapped_len, fd);

	if (dest <= src)
		UT_FATAL("cannot map files in memory order");

	do_memcpy_variants(fd, dest, dest_off, src, src_off,
		bytes, 0, argv[1], persist);

	int ret = pmem_unmap(dest_orig, mapped_len);
	UT_ASSERTeq(ret, 0);

	MUNMAP(src_orig, mapped_len);

	CLOSE(fd);

	DONE(NULL);
}
