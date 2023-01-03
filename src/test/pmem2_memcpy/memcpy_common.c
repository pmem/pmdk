// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * memcpy_common.c -- common part for tests doing a persistent memcpy
 */

#include "unittest.h"
#include "memcpy_common.h"
#include "valgrind_internal.h"

/*
 * do_persist - performs selected persist function
 */
static void
do_persist(persist_fn p, char *ptr, size_t len)
{
	p(ptr, len);
}

/*
 * do_memcpy_s - performs selected memcpy function
 */
static void *
do_memcpy_s(memcpy_fn m, char *ptr1, char *ptr2,
			size_t len, unsigned flags)
{
	return m(ptr1, ptr2, len, flags);
}

/*
 * do_memcpy: Worker function for memcpy
 *
 * Always work within the boundary of bytes. Fill in 1/2 of the src
 * memory with the pattern we want to write. This allows us to check
 * that we did not overwrite anything we were not supposed to in the
 * dest. Use the non pmem version of the memset/memcpy commands
 * so as not to introduce any possible side affects.
 */
void
do_memcpy(int fd, char *dest, int dest_off, char *src, int src_off,
    size_t bytes, size_t mapped_len, const char *file_name, memcpy_fn fn,
    unsigned flags, persist_fn persist)
{
	void *ret;
	char *buf = MALLOC(bytes);

	memset(buf, 0, bytes);
	memset(dest, 0, bytes);
	do_persist(persist, dest, bytes);
	memset(src, 0, bytes);
	do_persist(persist, src, bytes);

	memset(src, 0x5A, bytes / 4);
	do_persist(persist, src, bytes / 4);
	memset(src + bytes / 4, 0x46, bytes / 4);
	do_persist(persist, src + bytes / 4, bytes / 4);

	/* dest == src */
	ret = do_memcpy_s(fn, dest + dest_off, dest + dest_off,
		bytes / 2, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	/* len == 0 */
	ret = do_memcpy_s(fn, dest + dest_off, src, 0, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	ret = do_memcpy_s(fn, dest + dest_off, src + src_off,
		bytes / 2, flags);
	if (flags & PMEM2_F_MEM_NOFLUSH)
		VALGRIND_DO_PERSIST((dest + dest_off), bytes / 2);
	UT_ASSERTeq(ret, dest + dest_off);

	/* memcmp will validate that what I expect in memory. */
	if (memcmp(src + src_off, dest + dest_off, bytes / 2))
		UT_FATAL("%s: first %zu bytes do not match",
			file_name, bytes / 2);

	/* Now validate the contents of the file */
	LSEEK(fd, dest_off + (os_off_t)(mapped_len) / 2, SEEK_SET);
	if (READ(fd, buf, bytes / 2) == bytes / 2) {
		if (memcmp(src + src_off, buf, bytes / 2))
			UT_FATAL("%s: first %zu bytes do not match",
				file_name, bytes / 2);
	}

	FREE(buf);
}

unsigned Flags[] = {
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
