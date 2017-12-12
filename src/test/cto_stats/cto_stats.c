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
 * cto_stats.c -- unit test for cto_stats
 *
 * usage: cto_stats filename1 filename2 [opts]
 */

#include "unittest.h"

int
main(int argc, char *argv[])
{
	PMEMctopool *pcp1;
	PMEMctopool *pcp2;
	char *opts = "";

	START(argc, argv, "cto_stats");

	if (argc > 4) {
		UT_FATAL("usage: %s filename1 filename2 [opts]", argv[0]);
	} else {
		if (argc > 3)
			opts = argv[3];
	}

	pcp1 = pmemcto_create(argv[1], "test1", PMEMCTO_MIN_POOL, 0600);
	UT_ASSERTne(pcp1, NULL);
	pcp2 = pmemcto_create(argv[2], "test2", PMEMCTO_MIN_POOL, 0600);
	UT_ASSERTne(pcp2, NULL);

	int *ptr = pmemcto_malloc(pcp1, sizeof(int) * 100);
	UT_ASSERTne(ptr, NULL);

	pmemcto_stats_print(pcp1, opts);
	pmemcto_stats_print(pcp2, opts);

	pmemcto_close(pcp1);
	pmemcto_close(pcp2);

	pcp1 = pmemcto_open(argv[1], "test1");
	UT_ASSERTne(pcp1, NULL);
	pcp2 = pmemcto_open(argv[2], "test2");
	UT_ASSERTne(pcp2, NULL);

	pmemcto_stats_print(pcp1, opts);
	pmemcto_stats_print(pcp2, opts);

	pmemcto_free(pcp1, ptr);

	pmemcto_stats_print(pcp1, opts);
	pmemcto_stats_print(pcp2, opts);

	pmemcto_close(pcp1);
	pmemcto_close(pcp2);

	DONE(NULL);
}
