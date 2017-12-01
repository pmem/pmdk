/*
 * Copyright 2014-2017, Intel Corporation
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
 * cto_realloc_inplace -- unit test for pmemcto_realloc
 *
 * usage: cto_realloc_inplace filename
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_realloc_inplace");

	if (argc != 2)
		UT_FATAL("usage: %s filename", argv[0]);

	PMEMctopool *pcp = pmemcto_create(argv[1], "test",
			PMEMCTO_MIN_POOL, 0666);
	UT_ASSERTne(pcp, NULL);

	int *test1 = pmemcto_malloc(pcp, 12 * 1024 * 1024);
	UT_ASSERTne(test1, NULL);

	int *test1r = pmemcto_realloc(pcp, test1, 6 * 1024 * 1024);
	UT_ASSERTeq(test1r, test1);

	test1r = pmemcto_realloc(pcp, test1, 12 * 1024 * 1024);
	UT_ASSERTeq(test1r, test1);

	test1r = pmemcto_realloc(pcp, test1, 8 * 1024 * 1024);
	UT_ASSERTeq(test1r, test1);

	int *test2 = pmemcto_malloc(pcp, 4 * 1024 * 1024);
	UT_ASSERTne(test2, NULL);

	/* 4MB => 16B */
	int *test2r = pmemcto_realloc(pcp, test2, 16);
	UT_ASSERTeq(test2r, NULL);

	/* ... but the usable size is still 4MB. */
	UT_ASSERTeq(pmemcto_malloc_usable_size(pcp, test2), 4 * 1024 * 1024);

	/* 8MB => 16B */
	test1r = pmemcto_realloc(pcp, test1, 16);
	/*
	 * If the old size of the allocation is larger than
	 * the chunk size (4MB), we can reallocate it to 4MB first (in place),
	 * releasing some space, which makes it possible to do the actual
	 * shrinking...
	 */
	UT_ASSERTne(test1r, NULL);
	UT_ASSERTne(test1r, test1);
	UT_ASSERTeq(pmemcto_malloc_usable_size(pcp, test1r), 16);

	/* ... and leaves some memory for new allocations. */
	int *test3 = pmemcto_malloc(pcp, 3 * 1024 * 1024);
	UT_ASSERTne(test3, NULL);

	pmemcto_free(pcp, test1r);
	pmemcto_free(pcp, test2r);
	pmemcto_free(pcp, test3);

	pmemcto_close(pcp);

	DONE(NULL);
}
