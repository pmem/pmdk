// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2023, Intel Corporation */

/*
 * obj_lane.c -- unit test for lanes
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <errno.h>
#include <inttypes.h>

#include "list.h"
#include "obj.h"
#include "tx.h"
#include "unittest.h"
#include "pmemcommon.h"

#define MAX_MOCK_LANES 5

#define MOCK_LAYOUT (void *)(0xAAA)

static void *base_ptr;

struct mock_pop {
	PMEMobjpool p;
	struct lane_layout l[MAX_MOCK_LANES];
};

/*
 * mock_flush -- mock flush for lanes
 */
static int
mock_flush(void *ctx, const void *addr, size_t len, unsigned flags)
{
	return 0;
}

/*
 * mock_persist -- mock flush for lanes
 */
static int
mock_persist(void *ctx, const void *addr, size_t len, unsigned flags)
{
	return 0;
}

/*
 * mock_memset -- mock memset for lanes
 */
static void *
mock_memset(void *ctx, void *ptr, int c, size_t sz, unsigned flags)
{
	memset(ptr, c, sz);
	return ptr;
}

/*
 * mock_drain -- mock drain for lanes
 */
static void
mock_drain(void *ctx)
{

}

static void
test_lane_boot_cleanup_ok(void)
{
	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.nlanes = MAX_MOCK_LANES;

	base_ptr = &pop->p;

	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	pop->p.p_ops.base = pop;
	pop->p.p_ops.flush = mock_flush;
	pop->p.p_ops.memset = mock_memset;
	pop->p.p_ops.drain = mock_drain;
	pop->p.p_ops.persist = mock_persist;

	lane_init_data(&pop->p);
	lane_info_boot();
	UT_ASSERTeq(lane_boot(&pop->p), 0);

	for (int i = 0; i < MAX_MOCK_LANES; ++i) {
		struct lane *lane = &pop->p.lanes_desc.lane[i];
		UT_ASSERTeq(lane->layout, &pop->l[i]);
	}

	lane_cleanup(&pop->p);

	UT_ASSERTeq(pop->p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop->p.lanes_desc.lane_locks, NULL);

	FREE(pop);
}

static ut_jmp_buf_t Jmp;

static void
signal_handler(int sig)
{
	ut_siglongjmp(Jmp);
}

static void
test_lane_hold_release(void)
{
	struct ulog *mock_ulog = ZALLOC(SIZEOF_ULOG(1024));
	struct pmem_ops p_ops;
	struct operation_context *ctx = operation_new(mock_ulog, 1024,
		NULL, NULL, &p_ops, LOG_TYPE_REDO);

	struct lane mock_lane = {
		.layout = MOCK_LAYOUT,
		.internal = ctx,
		.external = ctx,
		.undo = ctx,
	};

	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));

	pop->p.nlanes = 1;
	pop->p.lanes_desc.runtime_nlanes = 1,
	pop->p.lanes_desc.lane = &mock_lane;
	pop->p.lanes_desc.next_lane_idx = 0;

	pop->p.lanes_desc.lane_locks = CALLOC(OBJ_NLANES, sizeof(uint64_t));
	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;
	pop->p.uuid_lo = 123456;
	base_ptr = &pop->p;

	struct lane *lane;
	lane_hold(&pop->p, &lane);
	UT_ASSERTeq(lane->layout, MOCK_LAYOUT);
	UT_ASSERTeq(lane->undo, ctx);

	lane_hold(&pop->p, &lane);
	UT_ASSERTeq(lane->layout, MOCK_LAYOUT);
	UT_ASSERTeq(lane->undo, ctx);

	lane_release(&pop->p);
	lane_release(&pop->p);
	struct sigaction v, old;
	sigemptyset(&v.sa_mask);
	v.sa_flags = 0;
	v.sa_handler = signal_handler;

	SIGACTION(SIGABRT, &v, &old);

	if (!ut_sigsetjmp(Jmp)) {
		lane_release(&pop->p); /* only two sections were held */
		UT_ERR("we should not get here");
	}

	SIGACTION(SIGABRT, &old, NULL);

	FREE(pop->p.lanes_desc.lane_locks);
	FREE(pop);
	operation_delete(ctx);
	FREE(mock_ulog);
}

static void
test_lane_sizes(void)
{
	UT_COMPILE_ERROR_ON(sizeof(struct lane_layout) != LANE_TOTAL_SIZE);
}

enum thread_work_type {
	LANE_INFO_DESTROY,
	LANE_CLEANUP
};

struct thread_data {
	enum thread_work_type work;
};

/*
 * test_separate_thread -- child thread input point for multithreaded
 *	scenarios
 */
static void *
test_separate_thread(void *arg)
{
	UT_ASSERTne(arg, NULL);

	struct thread_data *data = arg;

	switch (data->work) {
	case LANE_INFO_DESTROY:
		lane_info_destroy();
		break;
	case LANE_CLEANUP:
		UT_ASSERTne(base_ptr, NULL);
		lane_cleanup(base_ptr);
		break;
	default:
		UT_FATAL("Unimplemented thread work type: %d", data->work);
	}

	return NULL;
}

/*
 * test_lane_info_destroy_in_separate_thread -- lane info boot from one thread
 *	and lane info destroy from another
 */
static void
test_lane_info_destroy_in_separate_thread(void)
{
	lane_info_boot();

	struct thread_data data;
	data.work = LANE_INFO_DESTROY;
	os_thread_t thread;

	THREAD_CREATE(&thread, NULL, test_separate_thread, &data);
	THREAD_JOIN(&thread, NULL);

	lane_info_destroy();
}

/*
 * test_lane_cleanup_in_separate_thread -- lane boot from one thread and lane
 *	cleanup from another
 */
static void
test_lane_cleanup_in_separate_thread(void)
{
	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.nlanes = MAX_MOCK_LANES;

	pop->p.p_ops.base = pop;
	pop->p.p_ops.flush = mock_flush;
	pop->p.p_ops.memset = mock_memset;
	pop->p.p_ops.drain = mock_drain;
	pop->p.p_ops.persist = mock_persist;

	base_ptr = &pop->p;

	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	lane_init_data(&pop->p);
	lane_info_boot();
	UT_ASSERTeq(lane_boot(&pop->p), 0);

	for (int i = 0; i < MAX_MOCK_LANES; ++i) {
		struct lane *lane = &pop->p.lanes_desc.lane[i];
		UT_ASSERTeq(lane->layout, &pop->l[i]);
	}

	struct thread_data data;
	data.work = LANE_CLEANUP;
	os_thread_t thread;

	THREAD_CREATE(&thread, NULL, test_separate_thread, &data);
	THREAD_JOIN(&thread, NULL);

	UT_ASSERTeq(pop->p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop->p.lanes_desc.lane_locks, NULL);

	FREE(pop);
}

static void
test_fault_injection()
{
	if (!pmemobj_fault_injection_enabled())
		return;
	pmemobj_inject_fault_at(PMEM_MALLOC, 1, "lane_boot");

	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.p_ops.base = pop;

	int ret = lane_boot(&pop->p);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOMEM);
}

static void
usage(const char *app)
{
	UT_FATAL("usage: %s [scenario: s/m]", app);
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_lane");

	if (argc != 2)
		usage(argv[0]);

	switch (argv[1][0]) {
	case 's':
		/* single thread scenarios */
		test_lane_boot_cleanup_ok();
		test_lane_hold_release();
		test_lane_sizes();
		break;
	case 'm':
		/* multithreaded scenarios */
		test_lane_info_destroy_in_separate_thread();
		test_lane_cleanup_in_separate_thread();
		break;
	case 'f':
		/* fault injection */
		test_fault_injection();
		break;
	default:
		usage(argv[0]);
	}

	DONE(NULL);
}
