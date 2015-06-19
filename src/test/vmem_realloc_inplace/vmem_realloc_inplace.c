/*
 * Copyright (c) 2014,2015 Intel Corporation
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
 * vmem_realloc_inplace -- unit test for vmem_realloc
 *
 * usage: vmem_realloc_inplace [directory]
 */

#include "unittest.h"

static void *
allocate_aligned(size_t size, size_t alignment)
{
	void *d = MMAP(NULL, size + 2 * alignment, PROT_READ|PROT_WRITE,
				MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
	uintptr_t di = (uintptr_t)d;
	uintptr_t di_aligned = (di + alignment - 1) & ~(alignment - 1);
	int r;
	size_t sz;

	sz = di_aligned - di;
	if (sz) {
		r = MUNMAP(d, sz);
		ASSERTeq(r, 0);
	}

	sz = di + size + 2 * alignment - (di_aligned + size);
	if (sz) {
		r = MUNMAP((void *)di_aligned + size, sz);
		ASSERTeq(r, 0);
	}

	return (void *)di_aligned;
}

#define	POOL_SIZE 16 * 1024 * 1024

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
		FATAL("usage: %s [directory]", argv[0]);
	}

	if (dir == NULL) {
		/* allocate memory for function vmem_create_in_region() */
		mem_pool = allocate_aligned(POOL_SIZE, 4 << 20);
		vmp = vmem_create_in_region(mem_pool, POOL_SIZE);
		if (vmp == NULL)
			FATAL("!vmem_create_in_region");
	} else {
		vmp = vmem_create(dir, POOL_SIZE);
		if (vmp == NULL)
			FATAL("!vmem_create");
	}

	int *test = vmem_malloc(vmp, 12 * 1024 * 1024);
	ASSERTne(test, NULL);

	test = vmem_realloc(vmp, test, 6 * 1024 * 1024);
	ASSERTne(test, NULL);

	vmem_free(vmp, test);

	vmem_delete(vmp);

	DONE(NULL);
}
