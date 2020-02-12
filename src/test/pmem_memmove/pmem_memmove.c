// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */

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

/*
 * verify_contents -- verify that buffers match, if they don't - print contents
 * of both and abort the test
 */
static void
verify_contents(const char *file_name, int test,
		const char *buf1, const char *buf2,
		size_t len)
{
	if (memcmp(buf1, buf2, len) == 0)
		return;

	for (size_t i = 0; i < len; ++i)
		UT_ERR("%04zu 0x%02x 0x%02x %s", i, (uint8_t)buf1[i],
				(uint8_t)buf2[i],
				buf1[i] != buf2[i] ? "!!!" : "");
	UT_FATAL("%s %d: %zu bytes do not match with memcmp",
		file_name, test, len);
}

/*
 * do_memmove: Worker function for memmove.
 *
 * Always work within the boundary of bytes. Fill in 1/2 of the src
 * memory with the pattern we want to write. This allows us to check
 * that we did not overwrite anything we were not supposed to in the
 * dest. Use the non pmem version of the memset/memcpy commands
 * so as not to introduce any possible side affects.
 */
static void
do_memmove(int ddax, char *dst, char *src, const char *file_name,
		size_t dest_off, size_t src_off, size_t bytes,
		pmem_memmove_fn fn, unsigned flags)
{
	void *ret;
	char *srcshadow = MALLOC(dest_off + src_off + bytes);
	char *dstshadow = srcshadow;
	if (src != dst)
		dstshadow = MALLOC(dest_off + src_off + bytes);
	char old;

	memset(src, 0x11, bytes);
	memset(dst, 0x22, bytes);

	memset(src, 0x33, bytes / 4);
	memset(src + bytes / 4, 0x44, bytes / 4);

	util_persist_auto(ddax, src, bytes);
	util_persist_auto(ddax, dst, bytes);

	memcpy(srcshadow, src, bytes);
	memcpy(dstshadow, dst, bytes);

	/* TEST 1, dest == src */
	old = *(char *)(dst + dest_off);
	ret = fn(dst + dest_off, dst + dest_off, bytes / 2, flags);
	UT_ASSERTeq(ret, dst + dest_off);
	UT_ASSERTeq(*(char *)(dst + dest_off), old);

	/* do the same using regular memmove and verify that buffers match */
	memmove(dstshadow + dest_off, dstshadow + dest_off, bytes / 2);
	verify_contents(file_name, 0, dstshadow, dst, bytes);
	verify_contents(file_name, 1, srcshadow, src, bytes);

	/* TEST 2, len == 0 */
	old = *(char *)(dst + dest_off);
	ret = fn(dst + dest_off, src + src_off, 0, flags);
	UT_ASSERTeq(ret, dst + dest_off);
	UT_ASSERTeq(*(char *)(dst + dest_off), old);

	/* do the same using regular memmove and verify that buffers match */
	memmove(dstshadow + dest_off, srcshadow + src_off, 0);
	verify_contents(file_name, 2, dstshadow, dst, bytes);
	verify_contents(file_name, 3, srcshadow, src, bytes);

	/* TEST 3, len == bytes / 2 */
	ret = fn(dst + dest_off, src + src_off, bytes / 2, flags);
	UT_ASSERTeq(ret, dst + dest_off);
	if (flags & PMEM_F_MEM_NOFLUSH)
		/* for pmemcheck */
		util_persist_auto(ddax, dst + dest_off, bytes / 2);

	/* do the same using regular memmove and verify that buffers match */
	memmove(dstshadow + dest_off, srcshadow + src_off, bytes / 2);
	verify_contents(file_name, 4, dstshadow, dst, bytes);
	verify_contents(file_name, 5, srcshadow, src, bytes);

	FREE(srcshadow);
	if (dstshadow != srcshadow)
		FREE(dstshadow);
}

#define USAGE() do { UT_FATAL("usage: %s file  b:length [d:{offset}] "\
	"[s:{offset}] [o:{0|1}]", argv[0]); } while (0)

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

static void
do_memmove_variants(int ddax, char *dst, char *src, const char *file_name,
	size_t dest_off, size_t src_off, size_t bytes)
{
	do_memmove(ddax, dst, src, file_name, dest_off, src_off,
			bytes, pmem_memmove_persist_wrapper, 0);
	do_memmove(ddax, dst, src, file_name, dest_off, src_off,
			bytes, pmem_memmove_nodrain_wrapper, 0);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memmove(ddax, dst, src, file_name, dest_off, src_off,
				bytes, pmem_memmove, Flags[i]);
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

		do_memmove_variants(type == TYPE_DEVDAX, dst, src, argv[1],
				dst_off, src_off, bytes);

		/* dest > src */
		swap_mappings(&dst, &src, mapped_len, fd);

		if (dst <= src)
			UT_FATAL("cannot map files in memory order");

		do_memmove_variants(type == TYPE_DEVDAX, dst, src, argv[1],
				dst_off, src_off, bytes);

		int ret = pmem_unmap(dst, mapped_len);
		UT_ASSERTeq(ret, 0);

		MUNMAP(src, mapped_len);
	} else {
		/* use the same buffer for source and destination */
		dst = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL);
		if (dst == NULL)
			UT_FATAL("!Could not mmap %s: \n", argv[1]);

		memset(dst, 0, bytes);
		util_persist_auto(type == TYPE_DEVDAX, dst, bytes);
		do_memmove_variants(type == TYPE_DEVDAX, dst, dst, argv[1],
				dst_off, src_off, bytes);

		int ret = pmem_unmap(dst, mapped_len);
		UT_ASSERTeq(ret, 0);
	}

	CLOSE(fd);

	DONE(NULL);
}
