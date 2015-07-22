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
#include "lane.h"
#include "redo.h"
#include "list.h"
#include "obj.h"
#include "heap_layout.h"
#include "unittest.h"

#define	MOCK_POOL_SIZE PMEMOBJ_MIN_POOL
#define	MAX_ALLOCS 20
#define	TEST_MEGA_ALLOC_SIZE (1024 * 1024)
#define	TEST_HUGE_ALLOC_SIZE (255 * 1024)
#define	TEST_SMALL_ALLOC_SIZE (200)
#define	TEST_MEDIUM_ALLOC_SIZE (300)
#define	TEST_TINY_ALLOC_SIZE (64)
#define	TEST_RUNS 2

#define	MAX_MALLOC_FREE_LOOP 1000
#define	MALLOC_FREE_SIZE 8000

struct mock_pop {
	PMEMobjpool p;
	char lanes[LANE_SECTION_LEN];
	uint64_t ptr;
};

static struct mock_pop *addr;
static PMEMobjpool *mock_pop;

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

void
test_constructor(void *ptr, void *arg)
{
	struct foo *f = ptr;
	f->bar = (uintptr_t)arg;
}

void
test_oom_allocs(size_t size)
{
	uint64_t max_allocs = MOCK_POOL_SIZE / size;
	uint64_t *allocs = CALLOC(max_allocs, sizeof (*allocs));

	size_t count = 0;
	for (;;) {
		if (pmalloc(mock_pop, &addr->ptr, size)) {
			break;
		}
		ASSERT(addr->ptr != 0);
		allocs[count++] = addr->ptr;
	}

	for (int i = 0; i < count; ++i) {
		addr->ptr = allocs[i];
		pfree(mock_pop, &addr->ptr);
		ASSERT(addr->ptr == 0);
	}
	ASSERT(count != 0);
	FREE(allocs);
}

void
test_malloc_free_loop(size_t size)
{
	int err;
	for (int i = 0; i < MAX_MALLOC_FREE_LOOP; ++i) {
		err = pmalloc(mock_pop, &addr->ptr, size);
		ASSERTeq(err, 0);
		pfree(mock_pop, &addr->ptr);
	}
}

void
test_realloc(size_t org, size_t dest)
{
	int err;
	err = pmalloc(mock_pop, &addr->ptr, org);
	ASSERTeq(err, 0);
	ASSERT(pmalloc_usable_size(mock_pop, addr->ptr) >= org);
	err = prealloc(mock_pop, &addr->ptr, dest);
	ASSERTeq(err, 0);
	ASSERT(pmalloc_usable_size(mock_pop, addr->ptr) >= dest);
	err = pfree(mock_pop, &addr->ptr);
	ASSERTeq(err, 0);
}

void
test_mock_pool_allocs()
{
	addr = MALLOC(MOCK_POOL_SIZE);
	mock_pop = &addr->p;
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

	test_malloc_free_loop(MALLOC_FREE_SIZE);

	/*
	 * Allocating till OOM and freeing the objects in a loop for different
	 * buckets covers basically all code paths except error cases.
	 */
	test_oom_allocs(TEST_HUGE_ALLOC_SIZE);
	test_oom_allocs(TEST_TINY_ALLOC_SIZE);
	test_oom_allocs(TEST_HUGE_ALLOC_SIZE);
	test_oom_allocs(TEST_SMALL_ALLOC_SIZE);
	test_oom_allocs(TEST_MEGA_ALLOC_SIZE);

	test_realloc(TEST_SMALL_ALLOC_SIZE, TEST_MEDIUM_ALLOC_SIZE);
	test_realloc(TEST_HUGE_ALLOC_SIZE, TEST_MEGA_ALLOC_SIZE);

	FREE(addr);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_basic");

	for (int i = 0; i < TEST_RUNS; ++i)
		test_mock_pool_allocs();

	DONE(NULL);
}
