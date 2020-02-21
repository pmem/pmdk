// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * memmove_common.c -- common part for tests doing a persistent memmove
 */

#include "unittest.h"
#include "memmove_common.h"

/*
 * verify_contents -- verify that buffers match, if they don't - print contents
 * of both and abort the test
 */
void
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
void
do_memmove(char *dst, char *src, const char *file_name,
		size_t dest_off, size_t src_off, size_t bytes,
		memmove_fn fn, unsigned flags, persist_fn persist)
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

	persist(src, bytes);
	persist(dst, bytes);

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
		persist(dst + dest_off, bytes / 2);

	/* do the same using regular memmove and verify that buffers match */
	memmove(dstshadow + dest_off, srcshadow + src_off, bytes / 2);
	verify_contents(file_name, 4, dstshadow, dst, bytes);
	verify_contents(file_name, 5, srcshadow, src, bytes);

	FREE(srcshadow);
	if (dstshadow != srcshadow)
		FREE(dstshadow);
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
