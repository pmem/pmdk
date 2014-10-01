/*
 * Copyright (c) 2014, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * vmem_stats.c -- unit test for vmem_stats
 *
 * usage: vmem_stats [opts]
 */

#include "unittest.h"

#define	TEST_VALUE 1234

int
main(int argc, char *argv[])
{
	char *opts = "";
	VMEM *vmp;

	START(argc, argv, "vmem_stats");

	if (argc == 2) {
		opts = argv[1];
	} else if (argc > 2) {
		FATAL("usage: %s [opts]", argv[0]);
	}

	/* allocate memory for function vmem_pool_create_in_region() */
	void *mem_pool = MMAP(NULL, VMEM_MIN_POOL, PROT_READ|PROT_WRITE,
				MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

	vmp = vmem_pool_create_in_region(mem_pool, VMEM_MIN_POOL);
	if (vmp == NULL)
		FATAL("!vmem_pool_create_in_region");

	int *test = vmem_malloc(vmp, sizeof (int)*100);
	ASSERTne(test, NULL);

	*test = TEST_VALUE;
	ASSERTeq(*test, TEST_VALUE);

	ASSERTrange(test, mem_pool, VMEM_MIN_POOL);

	vmem_pool_stats_print(vmp, opts);

	vmem_free(vmp, test);

	vmem_pool_delete(vmp);

	DONE(NULL);
}
