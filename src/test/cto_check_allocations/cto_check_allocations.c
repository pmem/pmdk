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
 * cto_check_allocations -- unit test for cto_check_allocations
 *
 * usage: cto_check_allocations filename
 */

#include "unittest.h"

#define MAX_ALLOC_SIZE (4L * 1024L * 1024L)
#define NALLOCS 16

/* buffer for all allocation pointers */
static char *ptrs[NALLOCS];

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_check_allocations");

	if (argc != 2)
		UT_FATAL("usage: %s filename", argv[0]);

	for (size_t size = 8; size <= MAX_ALLOC_SIZE; size *= 2) {
		PMEMctopool *pcp = pmemcto_create(argv[1], "test",
				PMEMCTO_MIN_POOL, 0666);
		UT_ASSERTne(pcp, NULL);

		memset(ptrs, 0, sizeof(ptrs));

		int i;
		for (i = 0; i < NALLOCS; ++i) {
			ptrs[i] =  pmemcto_malloc(pcp, size);
			if (ptrs[i] == NULL) {
				/* out of memory in pool */
				break;
			}

			/* check that pointer came from mem_pool */
			UT_ASSERTrange(ptrs[i], pcp, PMEMCTO_MIN_POOL);

			/* fill each allocation with a unique value */
			memset(ptrs[i], (char)i, size);
		}

		UT_ASSERT((i > 0) && (i + 1 < MAX_ALLOC_SIZE));

		/* check for unexpected modifications of the data */
		for (i = 0; i < NALLOCS && ptrs[i] != NULL; ++i) {
			for (size_t j = 0; j < size; ++j)
				UT_ASSERTeq(ptrs[i][j], (char)i);
			pmemcto_free(pcp, ptrs[i]);
		}

		pmemcto_close(pcp);

		UNLINK(argv[1]);
	}

	DONE(NULL);
}
