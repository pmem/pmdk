// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

/*
 * pmem_memcpy.c -- unit test for doing a memcpy
 *
 * usage: pmem_memcpy file destoff srcoff length
 *
 */

#include "unittest.h"
#include "util_pmem.h"
#include "file.h"

typedef void *pmem_memcpy_fn(void *pmemdest, const void *src, size_t len,
		unsigned flags);

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

/*
 * swap_mappings - given to mmapped regions swap them.
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

	/* mmap destination using src addr as hint */
	td = MMAP(s, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

	MUNMAP(*dest, size);
	*dest = td;

	/* mmap src using original destination addr as a hint */
	ts = MMAP(d, size, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANONYMOUS,
		-1, 0);
	*src = ts;
}

/*
 * do_memcpy: Worker function for memcpy
 *
 * Always work within the boundary of bytes. Fill in 1/2 of the src
 * memory with the pattern we want to write. This allows us to check
 * that we did not overwrite anything we were not supposed to in the
 * dest.  Use the non pmem version of the memset/memcpy commands
 * so as not to introduce any possible side affects.
 */

static void
do_memcpy(int fd, char *dest, int dest_off, char *src, int src_off,
    size_t bytes, const char *file_name, pmem_memcpy_fn fn, unsigned flags)
{
	void *ret;
	char *buf = MALLOC(bytes);

	enum file_type type = util_fd_get_type(fd);
	if (type < 0)
		UT_FATAL("cannot check type of file with fd %d", fd);

	memset(buf, 0, bytes);
	memset(dest, 0, bytes);
	memset(src, 0, bytes);
	util_persist_auto(type == TYPE_DEVDAX, src, bytes);

	memset(src, 0x5A, bytes / 4);
	util_persist_auto(type == TYPE_DEVDAX, src, bytes / 4);
	memset(src + bytes / 4, 0x46, bytes / 4);
	util_persist_auto(type == TYPE_DEVDAX, src + bytes / 4,
			bytes / 4);

	/* dest == src */
	ret = fn(dest + dest_off, dest + dest_off, bytes / 2, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	/* len == 0 */
	ret = fn(dest + dest_off, src, 0, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	ret = fn(dest + dest_off, src + src_off, bytes / 2, flags);
	UT_ASSERTeq(ret, dest + dest_off);

	/* memcmp will validate that what I expect in memory. */
	if (memcmp(src + src_off, dest + dest_off, bytes / 2))
		UT_FATAL("%s: first %zu bytes do not match",
			file_name, bytes / 2);

	/* Now validate the contents of the file */
	LSEEK(fd, (os_off_t)dest_off, SEEK_SET);
	if (READ(fd, buf, bytes / 2) == bytes / 2) {
		if (memcmp(src + src_off, buf, bytes / 2))
			UT_FATAL("%s: first %zu bytes do not match",
				file_name, bytes / 2);
	}

	FREE(buf);
}

static unsigned Flags[] = {
		0,
		PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_NONTEMPORAL,
		PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL,
		PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_NODRAIN,
		PMEM_F_MEM_WC,
		PMEM_F_MEM_WB,
		PMEM_F_MEM_NOFLUSH,
		/* all possible flags */
		PMEM_F_MEM_NODRAIN | PMEM_F_MEM_NOFLUSH |
			PMEM_F_MEM_NONTEMPORAL | PMEM_F_MEM_TEMPORAL |
			PMEM_F_MEM_WC | PMEM_F_MEM_WB,
};

/*
 * do_memcpy_variants -- do_memcpy wrapper that tests multiple variants
 * of memcpy functions
 */
static void
do_memcpy_variants(int fd, char *dest, int dest_off, char *src, int src_off,
		    size_t bytes, const char *file_name)
{
	do_memcpy(fd, dest, dest_off, src, src_off, bytes, file_name,
			pmem_memcpy_persist_wrapper, 0);

	do_memcpy(fd, dest, dest_off, src, src_off, bytes, file_name,
			pmem_memcpy_nodrain_wrapper, 0);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memcpy(fd, dest, dest_off, src, src_off, bytes, file_name,
				pmem_memcpy, Flags[i]);
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

	memset(dest, 0, (2 * bytes));
	util_persist_auto(type == TYPE_DEVDAX, dest, 2 * bytes);
	memset(src, 0, (2 * bytes));

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes, argv[1]);

	/* dest > src */
	swap_mappings(&dest, &src, mapped_len, fd);

	if (dest <= src)
		UT_FATAL("cannot map files in memory order");

	do_memcpy_variants(fd, dest, dest_off, src, src_off, bytes, argv[1]);

	int ret = pmem_unmap(dest_orig, mapped_len);
	UT_ASSERTeq(ret, 0);

	MUNMAP(src_orig, mapped_len);

	CLOSE(fd);

	DONE(NULL);
}
