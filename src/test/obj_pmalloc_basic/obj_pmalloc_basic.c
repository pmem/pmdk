/*
 * Copyright (c) 2015, Intel Corporation
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
 * obj_pmalloc_basic.c -- unit test for pmalloc interface
 */
#include <stdint.h>

#include "libpmemobj.h"
#include "pmalloc.h"
#include "util.h"
#include "obj.h"

#include "unittest.h"

#define	MOCK_POOL_SIZE (10 * 1024 * 1024) /* 10 megabytes */
#define	TEST_ALLOC_SIZE 256
#define	TEST_ALLOC_GROW_SIZE (TEST_ALLOC_SIZE * 10)

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_basic");

	void *addr = MALLOC(MOCK_POOL_SIZE);
	PMEMobjpool *mock_pop = addr;
	mock_pop->addr = addr;
	mock_pop->size = MOCK_POOL_SIZE;
	mock_pop->rdonly = 0;
	mock_pop->is_pmem = 0;

	heap_boot(mock_pop);
	ASSERTne(mock_pop->heap, NULL);

	int ret;
	uint64_t off;
	ret = pmalloc(mock_pop->heap, &off, TEST_ALLOC_SIZE);
	ASSERTeq(ret, 0);
	ASSERT(off > 0 && off < MOCK_POOL_SIZE);

	ret = pgrow(mock_pop->heap, off, TEST_ALLOC_GROW_SIZE);
	ASSERTeq(ret, 0);

	size_t usable_size = pmalloc_usable_size(mock_pop->heap, off);
	ASSERT(usable_size >= TEST_ALLOC_GROW_SIZE);

	ret = pfree(mock_pop->heap, &off);
	ASSERTeq(ret, 0);
	ASSERTeq(off, 0);

	ret = heap_check(mock_pop);
	ASSERTeq(ret, 0);

	FREE(addr);

	DONE(NULL);
}
