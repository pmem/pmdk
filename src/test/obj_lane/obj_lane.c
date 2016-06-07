/*
 * Copyright 2015-2016, Intel Corporation
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

#include <pthread.h>
#include <errno.h>

#include "unittest.h"
#include "libpmemobj.h"
#include "util.h"
#include "lane.h"
#include "redo.h"
#include "heap_layout.h"
#include "memops.h"
#include "pmalloc.h"
#include "list.h"
#include "obj.h"
#include "pvector.h"
#include "tx.h"

#define MAX_MOCK_LANES 5
#define MOCK_RUNTIME (void *)(0xABC)
#define MOCK_RUNTIME_2 (void *)(0xBCD)

static void *base_ptr;
#define RPTR(p) (void *)((char *)p - (char *)base_ptr)

struct mock_pop {
	PMEMobjpool p;
	struct lane_layout l[MAX_MOCK_LANES];
};

static int construct_fail;

static int
lane_noop_construct(PMEMobjpool *pop, struct lane_section *section)
{
	UT_OUT("lane_noop_construct");
	if (construct_fail)
		return EINVAL;

	section->runtime = MOCK_RUNTIME;

	return 0;
}

static void
lane_noop_destruct(PMEMobjpool *pop, struct lane_section *section)
{
	UT_OUT("lane_noop_destruct");
}

static int recovery_check_fail;

static int
lane_noop_recovery(PMEMobjpool *pop,
	struct lane_section_layout *section)
{
	UT_OUT("lane_noop_recovery %p", RPTR(section));
	if (recovery_check_fail)
		return EINVAL;

	return 0;
}

static int
lane_noop_check(PMEMobjpool *pop,
	struct lane_section_layout *section)
{
	UT_OUT("lane_noop_check %p", RPTR(section));
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

struct section_operations noop_ops = {
	.construct = lane_noop_construct,
	.destruct = lane_noop_destruct,
	.recover = lane_noop_recovery,
	.check = lane_noop_check,
	.boot = lane_noop_boot
};

SECTION_PARM(LANE_SECTION_ALLOCATOR, &noop_ops);
SECTION_PARM(LANE_SECTION_LIST, &noop_ops);
SECTION_PARM(LANE_SECTION_TRANSACTION, &noop_ops);

static void
test_lane_boot_cleanup_ok()
{
	struct mock_pop pop = {
		.p = {
			.nlanes = MAX_MOCK_LANES
		}
	};
	base_ptr = &pop.p;

	pop.p.lanes_offset = (uint64_t)&pop.l - (uint64_t)&pop.p;

	lane_info_boot();
	UT_ASSERTeq(lane_boot(&pop.p), 0);

	for (int i = 0; i < MAX_MOCK_LANES; ++i) {
		for (int j = 0; j < MAX_LANE_SECTION; ++j) {
			struct lane_section *section =
				&pop.p.lanes_desc.lane[i].sections[j];
			UT_ASSERTeq(section->layout, &pop.l[i].sections[j]);
			UT_ASSERTeq(section->runtime, MOCK_RUNTIME);
		}
	}

	lane_cleanup(&pop.p);

	UT_ASSERTeq(pop.p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop.p.lanes_desc.lane_locks, NULL);
}

static void
test_lane_boot_fail()
{
	struct mock_pop pop = {
		.p = {
			.nlanes = MAX_MOCK_LANES
		}
	};
	base_ptr = &pop.p;
	pop.p.lanes_offset = (uint64_t)&pop.l - (uint64_t)&pop.p;

	construct_fail = 1;

	UT_ASSERTne(lane_boot(&pop.p), 0);

	construct_fail = 0;

	UT_ASSERTeq(pop.p.lanes_desc.lane, NULL);
	UT_ASSERTeq(pop.p.lanes_desc.lane_locks, NULL);
}

static void
test_lane_recovery_check_ok()
{
	struct mock_pop pop = {
		.p = {
			.nlanes = MAX_MOCK_LANES
		}
	};
	base_ptr = &pop.p;
	pop.p.lanes_offset = (uint64_t)&pop.l - (uint64_t)&pop.p;

	UT_ASSERTeq(lane_recover_and_section_boot(&pop.p), 0);
	UT_ASSERTeq(lane_check(&pop.p), 0);
}

static void
test_lane_recovery_check_fail()
{
	struct mock_pop pop = {
		.p = {
			.nlanes = MAX_MOCK_LANES
		}
	};
	base_ptr = &pop.p;
	pop.p.lanes_offset = (uint64_t)&pop.l - (uint64_t)&pop.p;

	recovery_check_fail = 1;

	UT_ASSERTne(lane_recover_and_section_boot(&pop.p), 0);

	UT_ASSERTne(lane_check(&pop.p), 0);
}

sigjmp_buf Jmp;

static void
signal_handler(int sig)
{
	siglongjmp(Jmp, 1);
}

static void
test_lane_hold_release()
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

	struct mock_pop pop = {
		.p = {
			.nlanes = 1,
			.lanes_desc = {
					.lane = &mock_lane,

					.next_lane_idx = 0
			}
		}
	};
	pop.p.lanes_desc.lane_locks = CALLOC(OBJ_NLANES, sizeof(uint64_t));
	pop.p.lanes_offset = (uint64_t)&pop.l - (uint64_t)&pop.p;
	base_ptr = &pop.p;

	struct lane_section *sec;
	lane_hold(&pop.p, &sec, LANE_SECTION_ALLOCATOR);
	UT_ASSERTeq(sec->runtime, MOCK_RUNTIME);
	lane_hold(&pop.p, &sec, LANE_SECTION_LIST);
	UT_ASSERTeq(sec->runtime, MOCK_RUNTIME_2);

	lane_release(&pop.p);
	lane_release(&pop.p);

	void *old = signal(SIGABRT, signal_handler);

	if (!sigsetjmp(Jmp, 1)) {
		lane_release(&pop.p); /* only two sections were held */
		UT_ERR("we should not get here");
	}

	signal(SIGABRT, old);
	FREE(pop.p.lanes_desc.lane_locks);
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

int
main(int argc, char *argv[])
{
	START(argc, argv, "obj_lane");

	test_lane_boot_cleanup_ok();
	test_lane_boot_fail();
	test_lane_recovery_check_ok();
	test_lane_recovery_check_fail();
	test_lane_hold_release();
	test_lane_sizes();

	DONE(NULL);
}
