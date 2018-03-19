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
 * cto_dirty -- unit test for detecting inconsistent pool
 *
 * usage: cto_dirty filename [phase]
 */

#include "unittest.h"

#define POOL_SIZE (2 * PMEMCTO_MIN_POOL)

#define EXIT_ON(x, y) do {\
	if ((x) == (y)) {\
		exit(1);\
	}\
} while (0)

int
main(int argc, char *argv[])
{
	START(argc, argv, "cto_dirty");

	if (argc < 2)
		UT_FATAL("usage: %s filename [phase]", argv[0]);

	PMEMctopool *pcp;
	int phase = 0;

	if (argc > 2) {
		phase = atoi(argv[2]);
		pcp = pmemcto_create(argv[1], "test", POOL_SIZE, 0666);
		UT_ASSERTne(pcp, NULL);
	} else {
		pcp = pmemcto_open(argv[1], "test");
		if (pcp == NULL) {
			UT_ERR("pmemcto_open: %s", pmemcto_errormsg());
			goto end;
		}
	}

	EXIT_ON(phase, 1);

	void *ptr = pmemcto_malloc(pcp, 16);
	UT_ASSERTne(ptr, NULL);

	pmemcto_set_root_pointer(pcp, ptr);

	EXIT_ON(phase, 2);

	pmemcto_free(pcp, ptr);
	pmemcto_set_root_pointer(pcp, NULL);

	pmemcto_close(pcp);

end:
	DONE(NULL);
}
