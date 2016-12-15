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
 * cto_aligned_alloc.c -- unit test for pmemcto_aligned_alloc
 *
 * usage: cto_aligned_alloc filename
 */

#include "unittest.h"

#define MAX_ALIGNMENT (4L * 1024L * 1024L)
#define NALLOCS 16

/* buffer for all allocation pointers */
static int *ptrs[NALLOCS];

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_aligned_alloc");

	if (argc != 2)
		UT_FATAL("usage: %s filename", argv[0]);

	/* test with address alignment from 2B to 4MB */
	size_t alignment;
	for (alignment = 2; alignment <= MAX_ALIGNMENT; alignment *= 2) {
		PMEMctopool *pcp = pmemcto_create(argv[1], "test",
				PMEMCTO_MIN_POOL, 0666);
		UT_ASSERTne(pcp, NULL);

		memset(ptrs, 0, sizeof(ptrs));

		int i;
		for (i = 0; i < NALLOCS; ++i) {
			ptrs[i] = pmemcto_aligned_alloc(pcp, alignment,
					sizeof(int));

			/* at least one allocation must succeed */
			UT_ASSERT(i != 0 || ptrs[i] != NULL);
			if (ptrs[i] == NULL) {
				/* out of memory in pool */
				break;
			}

			/* check that pointer came from mem_pool */
			UT_ASSERTrange(ptrs[i], pcp, PMEMCTO_MIN_POOL);

			/* check for correct address alignment */
			UT_ASSERTeq((uintptr_t)(ptrs[i]) & (alignment - 1), 0);

			/* ptr should be usable */
			*ptrs[i] = i;
			UT_ASSERTeq(*ptrs[i], i);
		}

		/* check for unexpected modifications of the data */
		for (i = 0; i < NALLOCS && ptrs[i] != NULL; ++i)
			pmemcto_free(pcp, ptrs[i]);

		pmemcto_close(pcp);

		UNLINK(argv[1]);
	}

	DONE(NULL);
}
