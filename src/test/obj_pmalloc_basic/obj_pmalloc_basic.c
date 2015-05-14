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
#include "list.h"
#include "obj.h"
#include "lane.h"

#include "unittest.h"

#define	MOCK_POOL_SIZE PMEMOBJ_MIN_POOL
#define	MAX_ALLOCS 20
#define	TEST_ALLOC_SIZE 256
#define	TEST_VALUE 5

struct mock_pop {
	PMEMobjpool p;
	char lanes[LANE_SECTION_LEN];
	uint64_t ptr;
};

/*
 * drain_empty -- (internal) empty function for drain on non-pmem memory
 */
static void
drain_empty(void)
{
	/* do nothing */
}

struct foo {
	uintptr_t bar;
};

void test_constructor(void *ptr, void *arg) {
	struct foo *f = ptr;
	f->bar = (uintptr_t)arg;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_basic");

	struct mock_pop *addr = MALLOC(MOCK_POOL_SIZE);
	PMEMobjpool *mock_pop = &addr->p;
	mock_pop->addr = addr;
	mock_pop->size = MOCK_POOL_SIZE;
	mock_pop->rdonly = 0;
	mock_pop->is_pmem = 0;
	mock_pop->heap_offset = sizeof (struct mock_pop);
	mock_pop->heap_size = MOCK_POOL_SIZE - mock_pop->heap_offset;
	mock_pop->persist = (persist_fn)pmem_msync;
	mock_pop->nlanes = 1;
	mock_pop->lanes_offset = sizeof (PMEMobjpool);
	mock_pop->flush = (flush_fn)pmem_msync;
	mock_pop->drain = drain_empty;

	lane_boot(mock_pop);

	heap_init(mock_pop);
	heap_boot(mock_pop);

	ASSERTne(mock_pop->heap, NULL);
	uint64_t addrs[MAX_ALLOCS];
	for (int i = 0; i < MAX_ALLOCS; ++i) {
		ASSERT(pmalloc(mock_pop, &addr->ptr, sizeof (struct foo)) == 0);
		addrs[i] = addr->ptr;
		ASSERT(addrs[i] != 0);
	}

	for (int i = 0; i < MAX_ALLOCS; ++i) {
		addr->ptr = addrs[i];
		ASSERT(pfree(mock_pop, &addr->ptr) == 0);
	}

	ASSERT(pmalloc_construct(mock_pop, &addr->ptr, sizeof (struct foo),
		test_constructor, (void *)TEST_VALUE, 0) == 0);

	struct foo *f = (void *)mock_pop + addr->ptr;
	ASSERT(f->bar == TEST_VALUE);

	FREE(addr);

	DONE(NULL);
}
