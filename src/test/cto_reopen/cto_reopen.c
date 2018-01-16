/*
 * Copyright 2014-2018, Intel Corporation
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
 * cto_reopen -- unit test for cto_reopen
 *
 * usage: cto_reopen filename nrep
 */

#include "unittest.h"

#define ALLOC_SIZE (1024L)
#define NALLOCS 16
#define POOL_SIZE (2 * PMEMCTO_MIN_POOL)

/* buffer for all allocation pointers */
static char *ptrs[NALLOCS];

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_reopen");

	if (argc != 3)
		UT_FATAL("usage: %s filename nrep", argv[0]);

	int nrep = atoi(argv[2]);

	PMEMctopool *pcp;
	for (int r = 0; r < nrep; r++) {
		if (r == 0) {
			pcp = pmemcto_create(argv[1], "test",
					POOL_SIZE, 0666);
		} else {
			pcp = pmemcto_open(argv[1], "test");
		}
		UT_ASSERTne(pcp, NULL);

		memset(ptrs, 0, sizeof(ptrs));

		int i;
		for (i = 0; i < NALLOCS; ++i) {
			ptrs[i] =  pmemcto_malloc(pcp, ALLOC_SIZE);
			if (ptrs[i] == NULL) {
				/* out of memory in pool */
				break;
			}

			/* check that pointer came from mem_pool */
			UT_ASSERTrange(ptrs[i], pcp, POOL_SIZE);

			/* fill each allocation with a unique value */
			memset(ptrs[i], (char)i, ALLOC_SIZE);
		}

		UT_OUT("rep %d cnt %d", r, i);
		UT_ASSERTeq(i, NALLOCS);

		for (i = 0; i < NALLOCS && ptrs[i] != NULL; ++i)
			pmemcto_free(pcp, ptrs[i]);

		pmemcto_close(pcp);
	}

	DONE(NULL);
}
