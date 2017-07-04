/*
 * Copyright 2015-2017, Intel Corporation
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
 * obj_pmalloc_basic.c -- unit test for pmalloc interface
 */
#include <stdint.h>

#include "heap.h"
#include "obj.h"
#include "pmalloc.h"
#include "unittest.h"
#include "valgrind_internal.h"

#define MOCK_POOL_SIZE (PMEMOBJ_MIN_POOL * 3)
#define TEST_MEGA_ALLOC_SIZE (10 * 1024 * 1024)
#define TEST_HUGE_ALLOC_SIZE (4 * 255 * 1024)
#define TEST_SMALL_ALLOC_SIZE (1000)
#define TEST_MEDIUM_ALLOC_SIZE (10000)
#define TEST_TINY_ALLOC_SIZE (64)
#define TEST_RUNS 2

#define MAX_MALLOC_FREE_LOOP 1000
#define MALLOC_FREE_SIZE 8000

struct mock_pop {
	PMEMobjpool p;
	char lanes[LANE_SECTION_LEN * MAX_LANE_SECTION];
	char padding[1024]; /* to page boundary */
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

/*
 * obj_persist -- pmemobj version of pmem_persist w/o replication
 */
static void
obj_persist(void *ctx, const void *addr, size_t len)
{
	PMEMobjpool *pop = ctx;
	pop->persist_local(addr, len);
}

/*
 * obj_flush -- pmemobj version of pmem_flush w/o replication
 */
static void
obj_flush(void *ctx, const void *addr, size_t len)
{
	PMEMobjpool *pop = ctx;
	pop->flush_local(addr, len);
}

/*
 * obj_drain -- pmemobj version of pmem_drain w/o replication
 */
static void
obj_drain(void *ctx)
{
	PMEMobjpool *pop = ctx;
	pop->drain_local();
}

/*
 * obj_memcpy -- pmemobj version of memcpy w/o replication
 */
static void *
obj_memcpy(void *ctx, void *dest, const void *src, size_t len)
{
	memcpy(dest, src, len);
	return dest;
}

static void *
obj_memset(void *ctx, void *ptr, int c, size_t sz)
{
	memset(ptr, c, sz);
	UT_ASSERTeq(pmem_msync(ptr, sz), 0);
	return ptr;
}

static void
test_oom_allocs(size_t size)
{
	uint64_t max_allocs = MOCK_POOL_SIZE / size;
	uint64_t *allocs = CALLOC(max_allocs, sizeof(*allocs));

	size_t count = 0;
	for (;;) {
		if (pmalloc(mock_pop, &addr->ptr, size, 0, 0)) {
			break;
		}
		UT_ASSERT(addr->ptr != 0);
		allocs[count++] = addr->ptr;
	}

	for (int i = 0; i < count; ++i) {
		addr->ptr = allocs[i];
		pfree(mock_pop, &addr->ptr);
		UT_ASSERT(addr->ptr == 0);
	}
	UT_ASSERT(count != 0);
	FREE(allocs);
}

static void
test_malloc_free_loop(size_t size)
{
	int err;
	for (int i = 0; i < MAX_MALLOC_FREE_LOOP; ++i) {
		err = pmalloc(mock_pop, &addr->ptr, size, 0, 0);
		UT_ASSERTeq(err, 0);
		pfree(mock_pop, &addr->ptr);
	}
}

static void
test_realloc(size_t org, size_t dest)
{
	int err;
	struct palloc_heap *heap = &mock_pop->heap;
	err = pmalloc(mock_pop, &addr->ptr, org, 0, 0);
	UT_ASSERTeq(err, 0);
	UT_ASSERT(palloc_usable_size(heap, addr->ptr) >= org);
	err = prealloc(mock_pop, &addr->ptr, dest, 0, 0);
	UT_ASSERTeq(err, 0);
	UT_ASSERT(palloc_usable_size(heap, addr->ptr) >= dest);
	pfree(mock_pop, &addr->ptr);
}

static int
redo_log_check_offset(void *ctx, uint64_t offset)
{
	PMEMobjpool *pop = ctx;
	return OBJ_OFF_IS_VALID(pop, offset);
}

#define MOCK_RUN_ID 5

#define PMALLOC_EXTRA 20
#define PALLOC_FLAG (1 << 15)

#define FIRST_SIZE 1 /* use the first allocation class */
#define FIRST_USIZE 112 /* the usable size is 128 - 16 */

static void
test_pmalloc_extras(PMEMobjpool *pop)
{
	uint64_t val;
	int ret = pmalloc(pop, &val, FIRST_SIZE, PMALLOC_EXTRA, PALLOC_FLAG);
	UT_ASSERTeq(ret, 0);

	UT_ASSERTeq(palloc_extra(&pop->heap, val), PMALLOC_EXTRA);
	UT_ASSERT((palloc_flags(&pop->heap, val) & PALLOC_FLAG) == PALLOC_FLAG);
	UT_ASSERT(palloc_usable_size(&pop->heap, val) == FIRST_USIZE);

	pfree(pop, &val);
}

#define PMALLOC_ELEMENTS 20

static void
test_pmalloc_first_next(PMEMobjpool *pop)
{
	uint64_t vals[PMALLOC_ELEMENTS];
	for (int i = 0; i < PMALLOC_ELEMENTS; ++i) {
		int ret = pmalloc(pop, &vals[i], FIRST_SIZE, i, i);
		UT_ASSERTeq(ret, 0);
	}

	uint64_t off = palloc_first(&pop->heap);
	UT_ASSERTne(off, 0);
	int nvalues = 0;
	do {
		UT_ASSERTeq(vals[nvalues], off);
		UT_ASSERTeq(palloc_extra(&pop->heap, off), nvalues);
		UT_ASSERTeq(palloc_flags(&pop->heap, off), nvalues);
		UT_ASSERT(palloc_usable_size(&pop->heap, off) == FIRST_USIZE);

		nvalues ++;
	} while ((off = palloc_next(&pop->heap, off)) != 0);
	UT_ASSERTeq(nvalues, PMALLOC_ELEMENTS);

	for (int i = 0; i < PMALLOC_ELEMENTS; ++i)
		pfree(pop, &vals[i]);
}

static void
test_mock_pool_allocs(void)
{
	addr = MMAP_ANON_ALIGNED(MOCK_POOL_SIZE, Ut_mmap_align);
	mock_pop = &addr->p;
	mock_pop->addr = addr;
	mock_pop->size = MOCK_POOL_SIZE;
	mock_pop->rdonly = 0;
	mock_pop->is_pmem = 0;
	mock_pop->heap_offset = offsetof(struct mock_pop, ptr);
	UT_ASSERTeq(mock_pop->heap_offset % Ut_pagesize, 0);
	mock_pop->heap_size = MOCK_POOL_SIZE - mock_pop->heap_offset;
	mock_pop->nlanes = 1;
	mock_pop->lanes_offset = sizeof(PMEMobjpool);
	mock_pop->is_master_replica = 1;

	mock_pop->persist_local = (persist_local_fn)pmem_msync;
	mock_pop->flush_local = (flush_local_fn)pmem_msync;
	mock_pop->drain_local = drain_empty;

	mock_pop->p_ops.persist = obj_persist;
	mock_pop->p_ops.flush = obj_flush;
	mock_pop->p_ops.drain = obj_drain;
	mock_pop->p_ops.memcpy_persist = obj_memcpy;
	mock_pop->p_ops.memset_persist = obj_memset;
	mock_pop->p_ops.base = mock_pop;
	mock_pop->p_ops.pool_size = mock_pop->size;

	mock_pop->redo = redo_log_config_new(addr, &mock_pop->p_ops,
			redo_log_check_offset, mock_pop, REDO_NUM_ENTRIES);

	void *heap_start = (char *)mock_pop + mock_pop->heap_offset;
	uint64_t heap_size = mock_pop->heap_size;

	heap_init(heap_start, heap_size, &mock_pop->p_ops);
	heap_boot(&mock_pop->heap, heap_start, heap_size, MOCK_RUN_ID, mock_pop,
			&mock_pop->p_ops);
	heap_buckets_init(&mock_pop->heap);

	/* initialize runtime lanes structure */
	mock_pop->lanes_desc.runtime_nlanes = (unsigned)mock_pop->nlanes;
	lane_boot(mock_pop);

	UT_ASSERTne(mock_pop->heap.rt, NULL);

	test_pmalloc_extras(mock_pop);
	test_pmalloc_first_next(mock_pop);

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

	lane_cleanup(mock_pop);
	redo_log_config_delete(mock_pop->redo);
	heap_cleanup(&mock_pop->heap);

	MUNMAP_ANON_ALIGNED(addr, MOCK_POOL_SIZE);
}

static void
test_spec_compliance(void)
{
	uint64_t max_alloc = MAX_MEMORY_BLOCK_SIZE -
		sizeof(struct allocation_header_legacy);

	UT_ASSERTeq(max_alloc, PMEMOBJ_MAX_ALLOC_SIZE);
	UT_COMPILE_ERROR_ON(offsetof(struct chunk_run, data) <
		MAX_CACHELINE_ALIGNMENT);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_pmalloc_basic");

	for (int i = 0; i < TEST_RUNS; ++i)
		test_mock_pool_allocs();

	test_spec_compliance();

	DONE(NULL);
}
