/*
 * Copyright 2014-2016, Intel Corporation
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
 * vmem_realloc_inplace -- unit test for vmem_realloc
 *
 * usage: vmem_realloc_inplace [directory]
 */

#include "unittest.h"

#define POOL_SIZE 16 * 1024 * 1024

int
main(int argc, char *argv[])
{
	char *dir = NULL;
	void *mem_pool = NULL;
	VMEM *vmp;

	START(argc, argv, "vmem_realloc_inplace");

	if (argc == 2) {
		dir = argv[1];
	} else if (argc > 2) {
		UT_FATAL("usage: %s [directory]", argv[0]);
	}

	if (dir == NULL) {
		/* allocate memory for function vmem_create_in_region() */
		mem_pool = MMAP_ANON_ALIGNED(POOL_SIZE, 4 << 20);
		vmp = vmem_create_in_region(mem_pool, POOL_SIZE);
		if (vmp == NULL)
			UT_FATAL("!vmem_create_in_region");
	} else {
		vmp = vmem_create(dir, POOL_SIZE);
		if (vmp == NULL)
			UT_FATAL("!vmem_create");
	}

	int *test1 = vmem_malloc(vmp, 12 * 1024 * 1024);
	UT_ASSERTne(test1, NULL);

	int *test1r = vmem_realloc(vmp, test1, 6 * 1024 * 1024);
	UT_ASSERTeq(test1r, test1);

	test1r = vmem_realloc(vmp, test1, 12 * 1024 * 1024);
	UT_ASSERTeq(test1r, test1);

	test1r = vmem_realloc(vmp, test1, 8 * 1024 * 1024);
	UT_ASSERTeq(test1r, test1);

	int *test2 = vmem_malloc(vmp, 4 * 1024 * 1024);
	UT_ASSERTne(test2, NULL);

	/* 4MB => 16B */
	int *test2r = vmem_realloc(vmp, test2, 16);
	/*
	 * There is no space left in the pool, so shrinking from huge to small
	 * size would normally fail (no space to allocate new arena chunk).
	 * However, we can return the pointer to the original allocation (not
	 * resized), which is still better than NULL...
	 */
	UT_ASSERTeq(test2r, test2);

	/* ... but the usable size is still 4MB. */
	UT_ASSERTeq(vmem_malloc_usable_size(vmp, test2r), 4 * 1024 * 1024);

	/* 8MB => 16B */
	test1r = vmem_realloc(vmp, test1, 16);
	/*
	 * If the old size of the allocation is larger than
	 * the chunk size (4MB), we can reallocate it to 4MB first (in place),
	 * releasing some space, which makes it possible to do the actual
	 * shrinking...
	 */
	UT_ASSERTne(test1r, NULL);
	UT_ASSERTne(test1r, test1);
	UT_ASSERTeq(vmem_malloc_usable_size(vmp, test1r), 16);

	/* ... and leaves some memory for new allocations. */
	int *test3 = vmem_malloc(vmp, 4 * 1024 * 1024);
	UT_ASSERTne(test3, NULL);

	vmem_free(vmp, test1r);
	vmem_free(vmp, test2r);
	vmem_free(vmp, test3);

	vmem_delete(vmp);

	DONE(NULL);
}
