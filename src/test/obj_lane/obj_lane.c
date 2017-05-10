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
#define MOCK_RUNTIME (void *)(0xABC)
#define MOCK_RUNTIME_2 (void *)(0xBCD)

#define LOG_PREFIX "trace"
#define LOG_LEVEL_VAR "TRACE_LOG_LEVEL"
#define LOG_FILE_VAR "TRACE_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

static void *base_ptr;
#define RPTR(p) (uintptr_t)((char *)p - (char *)base_ptr)

struct mock_pop {
	PMEMobjpool p;
	struct lane_layout l[MAX_MOCK_LANES];
};

static int construct_fail;

static void *
lane_noop_construct_rt(PMEMobjpool *pop)
{
	UT_OUT("lane_noop_construct");
	if (construct_fail) {
		errno = EINVAL;
		return NULL;
	}

	return MOCK_RUNTIME;
}

static void
lane_noop_destroy_rt(PMEMobjpool *pop, void *rt)
{
	UT_OUT("lane_noop_destruct");
}

static int recovery_check_fail;

static int
lane_noop_recovery(PMEMobjpool *pop, void *data, unsigned length)
{
	UT_OUT("lane_noop_recovery 0x%"PRIxPTR, RPTR(data));
	if (recovery_check_fail)
		return EINVAL;

	return 0;
}

static int
lane_noop_check(PMEMobjpool *pop, void *data, unsigned length)
{
	UT_OUT("lane_noop_check 0x%"PRIxPTR, RPTR(data));
	if (recovery_check_fail)
		return EINVAL;

	return 0;
}

static int
lane_noop_boot(PMEMobjpool *pop)
{
	UT_OUT("lane_noop_init");

	return 0;
}

static struct section_operations noop_ops = {
	.construct_rt = lane_noop_construct_rt,
	.destroy_rt = lane_noop_destroy_rt,
	.recover = lane_noop_recovery,
	.check = lane_noop_check,
	.boot = lane_noop_boot
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &noop_ops);
SECTION_PARM(LANE_SECTION_LIST, &noop_ops);
SECTION_PARM(LANE_SECTION_TRANSACTION, &noop_ops);

static void
test_lane_boot_cleanup_ok(void)
{
	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.nlanes = MAX_MOCK_LANES;

	base_ptr = &pop->p;

	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	lane_info_boot();
	UT_ASSERTeq(lane_boot(&pop->p), 0);

	for (int i = 0; i < MAX_MOCK_LANES; ++i) {
		for (int j = 0; j < MAX_LANE_SECTION; ++j) {
			struct lane_section *section =
				&pop->p.lanes_desc.lane[i].sections[j];
			UT_ASSERTeq(section->layout, &pop->l[i].sections[j]);
			UT_ASSERTeq(section->runtime, MOCK_RUNTIME);
		}
	}

	lane_cleanup(&pop->p);

	UT_ASSERTeq(pop->p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop->p.lanes_desc.lane_locks, NULL);

	FREE(pop);
}

static void
test_lane_boot_fail(void)
{
	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.nlanes = MAX_MOCK_LANES;

	base_ptr = &pop->p;
	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	construct_fail = 1;

	UT_ASSERTne(lane_boot(&pop->p), 0);

	construct_fail = 0;

	UT_ASSERTeq(pop->p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop->p.lanes_desc.lane_locks, NULL);

	FREE(pop);
}

static void
test_lane_recovery_check_ok(void)
{
	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.nlanes = MAX_MOCK_LANES;

	base_ptr = &pop->p;
	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	UT_ASSERTeq(lane_recover_and_section_boot(&pop->p), 0);
	UT_ASSERTeq(lane_check(&pop->p), 0);

	FREE(pop);
}

static void
test_lane_recovery_check_fail(void)
{
	struct mock_pop *pop = MALLOC(sizeof(struct mock_pop));
	pop->p.nlanes = MAX_MOCK_LANES;

	base_ptr = &pop->p;
	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	recovery_check_fail = 1;

	UT_ASSERTne(lane_recover_and_section_boot(&pop->p), 0);
	UT_ASSERTne(lane_check(&pop->p), 0);

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
	struct lane mock_lane = {
		.sections = {
			[LANE_SECTION_ALLOCATOR] = {
				.runtime = MOCK_RUNTIME
			},
			[LANE_SECTION_LIST] = {
				.runtime = MOCK_RUNTIME_2
			}
		}
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

	struct lane_section *sec;
	lane_hold(&pop->p, &sec, LANE_SECTION_ALLOCATOR);
	UT_ASSERTeq(sec->runtime, MOCK_RUNTIME);
	lane_hold(&pop->p, &sec, LANE_SECTION_LIST);
	UT_ASSERTeq(sec->runtime, MOCK_RUNTIME_2);

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
}

static void
test_lane_sizes(void)
{
	UT_COMPILE_ERROR_ON(sizeof(struct lane_tx_layout) > LANE_SECTION_LEN);
	UT_COMPILE_ERROR_ON(sizeof(struct lane_alloc_layout) >
				LANE_SECTION_LEN);
	UT_COMPILE_ERROR_ON(sizeof(struct lane_list_layout) >
				LANE_SECTION_LEN);
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

	os_thread_create(&thread, NULL, test_separate_thread, &data);
	os_thread_join(thread, NULL);

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

	base_ptr = &pop->p;

	pop->p.lanes_offset = (uint64_t)&pop->l - (uint64_t)&pop->p;

	lane_info_boot();
	UT_ASSERTeq(lane_boot(&pop->p), 0);

	for (int i = 0; i < MAX_MOCK_LANES; ++i) {
		for (int j = 0; j < MAX_LANE_SECTION; ++j) {
			struct lane_section *section =
				&pop->p.lanes_desc.lane[i].sections[j];
			UT_ASSERTeq(section->layout, &pop->l[i].sections[j]);
			UT_ASSERTeq(section->runtime, MOCK_RUNTIME);
		}
	}

	struct thread_data data;
	data.work = LANE_CLEANUP;
	os_thread_t thread;

	os_thread_create(&thread, NULL, test_separate_thread, &data);
	os_thread_join(thread, NULL);

	UT_ASSERTeq(pop->p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop->p.lanes_desc.lane_locks, NULL);

	FREE(pop);
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

	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
		MAJOR_VERSION, MINOR_VERSION);

	if (argc != 2)
		usage(argv[0]);

	switch (argv[1][0]) {
	case 's':
		/* single thread scenarios */
		test_lane_boot_cleanup_ok();
		test_lane_boot_fail();
		test_lane_recovery_check_ok();
		test_lane_recovery_check_fail();
		test_lane_hold_release();
		test_lane_sizes();
		break;
	case 'm':
		/* multithreaded scenarios */
		test_lane_info_destroy_in_separate_thread();
		test_lane_cleanup_in_separate_thread();
		break;
	default:
		usage(argv[0]);
	}

	DONE(NULL);
}
