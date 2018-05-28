/*
 * Copyright 2018, Intel Corporation
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
 * pmem_unmap.c -- unit tests for pmem_unmap
 */

#include "unittest.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

#define PAGE_4K (4 * KILOBYTE)
#define PAGE_2M (2 * MEGABYTE)

int
main(int argc, char *argv[])
{
	const char *path;
	unsigned long long len;
	int flags;
	mode_t mode;
	size_t mlenp;
	int is_pmem;
	char *ret;

	if (argc < 2)
		UT_FATAL("usage: %s path", argv[0]);

	path = argv[1];
	len = 0;
	flags = 0;
	mode = S_IWUSR | S_IRUSR;

	ret = pmem_map_file(path, len, flags, mode, &mlenp, &is_pmem);
	UT_ASSERTeq(pmem_unmap(ret, PAGE_4K), 0);

	ret = ret + PAGE_2M;
	UT_ASSERTeq(pmem_unmap(ret, PAGE_2M), 0);

	ret = ret + 8 * PAGE_2M;
	UT_ASSERTne(pmem_unmap(ret, 0), 0);

	ret = ret + PAGE_2M - 1;
	UT_ASSERTne(pmem_unmap(ret, PAGE_4K), 0);

	return 0;
}
