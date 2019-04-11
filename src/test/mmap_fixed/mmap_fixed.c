/*
 * Copyright 2016-2019, Intel Corporation
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
 * mmap_fixed.c -- test memory mapping with MAP_FIXED for various lengths
 *
 * This test is intended to be used for testing Windows implementation
 * of memory mapping routines - mmap(), munmap(), msync() and mprotect().
 * Those functions should provide the same functionality as their Linux
 * counterparts, at least with respect to the features that are used
 * in PMDK libraries.
 *
 * Known issues and differences between Linux and Windows implementation
 * are described in src/common/mmap_windows.c.
 */

#include "unittest.h"
#include <sys/mman.h>

#define ALIGN(size) ((size) & ~(Ut_mmap_align - 1))

/*
 * test_mmap_fixed -- test fixed mappings
 */
static void
test_mmap_fixed(const char *name1, const char *name2, size_t len1, size_t len2)
{
	size_t len1_aligned = ALIGN(len1);
	size_t len2_aligned = ALIGN(len2);

	UT_OUT("len: %zu (%zu) + %zu (%zu) = %zu", len1, len1_aligned,
		len2, len2_aligned, len1_aligned + len2_aligned);

	int fd1 = OPEN(name1, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR);
	int fd2 = OPEN(name2, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR);

	POSIX_FALLOCATE(fd1, 0, (os_off_t)len1);
	POSIX_FALLOCATE(fd2, 0, (os_off_t)len2);

	char *ptr1 = mmap(NULL, len1_aligned + len2_aligned,
		PROT_READ|PROT_WRITE, MAP_SHARED, fd1, 0);
	UT_ASSERTne(ptr1, MAP_FAILED);

	UT_OUT("ptr1: %p, ptr2: %p", ptr1, ptr1 + len1_aligned);

	char *ptr2 = mmap(ptr1 + len1_aligned, len2_aligned,
		PROT_READ|PROT_WRITE, MAP_FIXED|MAP_SHARED, fd2, 0);
	UT_ASSERTne(ptr2, MAP_FAILED);

	UT_ASSERTeq(ptr2, ptr1 + len1_aligned);

	UT_ASSERTne(munmap(ptr1, len1_aligned), -1);
	UT_ASSERTne(munmap(ptr2, len2_aligned), -1);

	CLOSE(fd1);
	CLOSE(fd2);

	UNLINK(name1);
	UNLINK(name2);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "mmap_fixed");

	if (argc < 4)
		UT_FATAL("usage: %s dirname len1 len2 ...", argv[0]);

	size_t *lengths = MALLOC(sizeof(size_t) * (size_t)argc - 2);
	UT_ASSERTne(lengths, NULL);

	size_t appendix_length = 20; /* a file name length */
	char *name1 = MALLOC(strlen(argv[1]) + appendix_length);
	char *name2 = MALLOC(strlen(argv[1]) + appendix_length);

	sprintf(name1, "%s\\testfile1", argv[1]);
	sprintf(name2, "%s\\testfile2", argv[1]);

	for (int i = 0; i < argc - 2; i++)
		lengths[i] = ATOULL(argv[i + 2]);

	for (int i = 0; i < argc - 2; i++)
		for (int j = 0; j < argc - 2; j++)
			test_mmap_fixed(name1, name2, lengths[i], lengths[j]);

	FREE(name1);
	FREE(name2);
	FREE(lengths);

	DONE(NULL);
}
