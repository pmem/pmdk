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
 * vmem_multiple_pools.c -- unit test for vmem_multiple_pools
 *
 * usage: vmem_multiple_pools directory
 */

#include "unittest.h"

#define	TEST_POOLS_MAX (9)
#define	TEST_REPEAT_CREATE_POOLS (30)

int
main(int argc, char *argv[])
{
	const unsigned mem_pools_size = TEST_POOLS_MAX/2 + TEST_POOLS_MAX%2;
	char *mem_pools[mem_pools_size];
	VMEM *pools[TEST_POOLS_MAX];
	memset(pools, 0, sizeof (pools[0]) * TEST_POOLS_MAX);

	START(argc, argv, "vmem_multiple_pools");

	if (argc < 2 || argc > 3)
		FATAL("usage: %s directory", argv[0]);

	const char *dir = argv[1];

	/* create and destroy pools multiple times */
	size_t repeat;
	size_t pool_id;

	for (pool_id = 0; pool_id < mem_pools_size; ++pool_id) {
		/* allocate memory for function vmem_pool_create_in_region() */
		mem_pools[pool_id] = MMAP(NULL, VMEM_MIN_POOL,
			PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	}

	for (repeat = 0; repeat < TEST_REPEAT_CREATE_POOLS; ++repeat) {
		for (pool_id = 0; pool_id < TEST_POOLS_MAX; ++pool_id) {

			/* delete old pool with this same id if exist */
			if (pools[pool_id] != NULL) {
				vmem_pool_delete(pools[pool_id]);
				pools[pool_id] = NULL;
			}

			if (pool_id % 2 == 0) {
				/* for even pool_id, create in region */
				pools[pool_id] = vmem_pool_create_in_region(
					mem_pools[pool_id / 2], VMEM_MIN_POOL);
				if (pools[pool_id] == NULL)
					FATAL("!vmem_pool_create_in_region");
			} else {
				/* for odd pool_id, create in file */
				pools[pool_id] = vmem_pool_create(dir,
					VMEM_MIN_POOL);
				if (pools[pool_id] == NULL)
					FATAL("!vmem_pool_create");
			}

			void *test = vmem_malloc(pools[pool_id],
				sizeof (void *));

			ASSERTne(test, NULL);
			vmem_free(pools[pool_id], test);
		}
	}

	for (pool_id = 0; pool_id < TEST_POOLS_MAX; ++pool_id) {
		if (pools[pool_id] != NULL) {
			vmem_pool_delete(pools[pool_id]);
			pools[pool_id] = NULL;
		}
	}

	DONE(NULL);
}
