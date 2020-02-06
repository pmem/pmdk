// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * pmem_memset.c -- unit test for doing a memset
 *
 * usage: pmem_memset file offset length
 */

#include "unittest.h"
#include "util_pmem.h"
#include "file.h"
#include "memset_common.h"

typedef void *pmem_memset_fn(void *pmemdest, int c, size_t len, unsigned flags);

static void *
pmem_memset_persist_wrapper(void *pmemdest, int c, size_t len, unsigned flags)
{
	(void) flags;
	return pmem_memset_persist(pmemdest, c, len);
}

static void *
pmem_memset_nodrain_wrapper(void *pmemdest, int c, size_t len, unsigned flags)
{
	(void) flags;
	return pmem_memset_nodrain(pmemdest, c, len);
}

static void
do_memset_variants(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, persist_fn p)
{
	do_memset(fd, dest, file_name, dest_off, bytes,
			pmem_memset_persist_wrapper, 0, p);

	do_memset(fd, dest, file_name, dest_off, bytes,
			pmem_memset_nodrain_wrapper, 0, p);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memset(fd, dest, file_name, dest_off, bytes,
				pmem_memset, Flags[i], p);
		if (Flags[i] & PMEMOBJ_F_MEM_NOFLUSH)
			pmem_persist(dest, bytes);
	}
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

int
main(int argc, char *argv[])
{
	int fd;
	size_t mapped_len;
	char *dest;

	if (argc != 4)
		UT_FATAL("usage: %s file offset length", argv[0]);

	const char *thr = os_getenv("PMEM_MOVNT_THRESHOLD");
	const char *avx = os_getenv("PMEM_AVX");
	const char *avx512f = os_getenv("PMEM_AVX512F");

	START(argc, argv, "pmem_memset %s %s %s %savx %savx512f",
			argv[2], argv[3],
			thr ? thr : "default",
			avx ? "" : "!",
			avx512f ? "" : "!");

	fd = OPEN(argv[1], O_RDWR);

	/* open a pmem file and memory map it */
	if ((dest = pmem_map_file(argv[1], 0, 0, 0, &mapped_len, NULL)) == NULL)
		UT_FATAL("!Could not mmap %s\n", argv[1]);

	size_t dest_off = strtoul(argv[2], NULL, 0);
	size_t bytes = strtoul(argv[3], NULL, 0);

	enum file_type type = util_fd_get_type(fd);
	if (type < 0)
		UT_FATAL("cannot check type of file with fd %d", fd);

	persist_fn p;
	p = type == TYPE_DEVDAX ? do_persist_ddax : do_persist;
	do_memset_variants(fd, dest, argv[1], dest_off, bytes, p);

	UT_ASSERTeq(pmem_unmap(dest, mapped_len), 0);

	CLOSE(fd);

	DONE(NULL);
}
