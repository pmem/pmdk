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
 * vmem_out_of_memory -- unit test for vmem_out_of_memory
 *
 * usage: vmem_out_of_memory [directory]
 */

#include "unittest.h"

static char mem_pool[VMEM_MIN_POOL];

int
main(int argc, char *argv[])
{
	char *dir = NULL;
	VMEM *vmp;
	START(argc, argv, "vmem_out_of_memory");

	if (argc == 2) {
		dir = argv[1];
	} else if (argc > 2) {
		FATAL("usage: %s [directory]", argv[0]);
	}

	if (dir == NULL) {
		vmp = vmem_pool_create_in_region(mem_pool, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_pool_create_in_region");
	} else {
		vmp = vmem_pool_create(dir, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_pool_create");
	}

	/* allocate all memory */
	void *prev = NULL;
	for (;;) {
		void **next = vmem_malloc(vmp, sizeof (void *));
		if (next == NULL) {
			/* out of memory */
			break;
		}

		/* check that pointer came from mem_pool */
		if (dir == NULL) {
			ASSERTrange(next, mem_pool, VMEM_MIN_POOL);
		}

		*next = prev;
		prev = next;
	}

	ASSERTne(prev, NULL);

	/* free all allocations */
	while (prev != NULL) {
		void **act = prev;
		prev = *act;
		vmem_free(vmp, act);
	}

	vmem_pool_delete(vmp);

	DONE(NULL);
}
