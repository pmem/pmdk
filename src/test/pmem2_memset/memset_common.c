// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2020, Intel Corporation */

/*
 * memset_common.c -- common part for tests doing a persistent memset
 */

#include "unittest.h"
#include "memset_common.h"

/*
 * do_memset - worker function for memset
 */
void
do_memset(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, memset_fn fn, unsigned flags,
		persist_fn persist)
{
	char *buf = MALLOC(bytes);
	char *dest1;
	char *ret;

	memset(dest, 0, bytes);
	persist(dest, bytes);
	dest1 = MALLOC(bytes);
	memset(dest1, 0, bytes);

	/*
	 * This is used to verify that the value of what a non persistent
	 * memset matches the outcome of the persistent memset. The
	 * persistent memset will match the file but may not be the
	 * correct or expected value.
	 */
	memset(dest1 + dest_off, 0x5A, bytes / 4);
	memset(dest1 + dest_off  + (bytes / 4), 0x46, bytes / 4);

	/* Test the corner cases */
	ret = fn(dest + dest_off, 0x5A, 0, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	UT_ASSERTeq(*(char *)(dest + dest_off), 0);

	/*
	 * Do the actual memset with persistence.
	 */
	ret = fn(dest + dest_off, 0x5A, bytes / 4, flags);
	UT_ASSERTeq(ret, dest + dest_off);
	ret = fn(dest + dest_off  + (bytes / 4), 0x46, bytes / 4, flags);
	UT_ASSERTeq(ret, dest + dest_off + (bytes / 4));

	if (memcmp(dest, dest1, bytes / 2))
		UT_FATAL("%s: first %zu bytes do not match",
				file_name, bytes / 2);

	LSEEK(fd, 0, SEEK_SET);
	if (READ(fd, buf, bytes / 2) == bytes / 2) {
		if (memcmp(buf, dest, bytes / 2))
			UT_FATAL("%s: first %zu bytes do not match",
					file_name, bytes / 2);
	}

	FREE(dest1);
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
