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
 * vmem_freespace -- unit test for vmem_freespace
 *
 * usage: vmem_freespace [directory]
 */

#include "unittest.h"

#define	MB (1024L * 1024L)

int
main(int argc, char *argv[])
{
	char *dir = NULL;
	VMEM *vmp;
	START(argc, argv, "vmem_freespace");

	if (argc == 2) {
		dir = argv[1];
	} else if (argc > 2) {
		FATAL("usage: %s [directory]", argv[0]);
	}

	if (dir == NULL) {
		/* allocate memory for function vmem_pool_create_in_region() */
		void *mem_pool = MMAP(NULL, VMEM_MIN_POOL, PROT_READ|PROT_WRITE,
					MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);

		vmp = vmem_pool_create_in_region(mem_pool, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_pool_create_in_region");
	} else {
		vmp = vmem_pool_create(dir, VMEM_MIN_POOL);
		if (vmp == NULL)
			FATAL("!vmem_pool_create");
	}

	size_t total_space = vmem_pool_freespace(vmp);
	size_t free_space = total_space;

	/* allocate all memory */
	void *prev = NULL;
	void **next;
	while ((next = vmem_malloc(vmp, 128)) != NULL) {
		*next = prev;
		prev = next;
		size_t space = vmem_pool_freespace(vmp);
		/* free space can only decrease */
		ASSERT(space <= free_space);
		free_space = space;
	}

	ASSERTne(prev, NULL);
	/* for small allocations use all memory */
	ASSERTeq(free_space, 0);

	while (prev != NULL) {
		void **act = prev;
		prev = *act;
		vmem_free(vmp, act);
		size_t space = vmem_pool_freespace(vmp);
		/* free space can only increase */
		ASSERT(space >= free_space);
		free_space = space;
	}

	free_space = vmem_pool_freespace(vmp);

	/*
	 * Depending on the distance of the 'mem_pool' from the
	 * chunk alignment (4MB) a different size of free memory
	 * will be wasted on base_alloc inside jemalloc.
	 * Rest of the internal data should not waste more than 10% of space.
	 */
	ASSERT(free_space > ((total_space - 4L * MB) * 9) / 10);

	vmem_pool_delete(vmp);

	DONE(NULL);
}
