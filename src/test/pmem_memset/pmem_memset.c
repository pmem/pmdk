/*
 * Copyright 2015-2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * pmem_memset.c -- unit test for doing a memset
 *
 * usage: pmem_memset file offset length
 */

#include "unittest.h"
#include "util_pmem.h"
#include "file.h"

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
do_memset(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes, pmem_memset_fn fn, unsigned flags)
{
	char *buf = MALLOC(bytes);
	char *dest1;
	char *ret;

	enum file_type type = util_fd_get_type(fd);
	if (type < 0)
		UT_FATAL("cannot check type of file with fd %d", fd);

	memset(dest, 0, bytes);
	util_persist_auto(type == TYPE_DEVDAX, dest, bytes);
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
do_memset_variants(int fd, char *dest, const char *file_name, size_t dest_off,
		size_t bytes)
{
	do_memset(fd, dest, file_name, dest_off, bytes,
			pmem_memset_persist_wrapper, 0);

	do_memset(fd, dest, file_name, dest_off, bytes,
			pmem_memset_nodrain_wrapper, 0);

	for (int i = 0; i < ARRAY_SIZE(Flags); ++i) {
		do_memset(fd, dest, file_name, dest_off, bytes,
				pmem_memset, Flags[i]);
		if (Flags[i] & PMEMOBJ_F_MEM_NOFLUSH)
			pmem_persist(dest, bytes);
	}
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

	do_memset_variants(fd, dest, argv[1], dest_off, bytes);

	UT_ASSERTeq(pmem_unmap(dest, mapped_len), 0);

	CLOSE(fd);

	DONE(NULL);
}
