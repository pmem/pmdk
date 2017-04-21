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
 * lane.h -- internal definitions for lanes
 */

#ifndef LIBPMEMOBJ_LANE_H
#define LIBPMEMOBJ_LANE_H 1

#include <stdint.h>

#include "libpmemobj.h"
#include "redo.h"

#define LANE_SECTION_LEN 1024

#define REDO_NUM_ENTRIES \
	((LANE_SECTION_LEN - 2 * sizeof(uint64_t)) / sizeof(struct redo_log))

/*
 * Distance between lanes used by threads required to prevent threads from
 * false sharing part of lanes array. Used if properly spread lanes are
 * available. Otherwise less spread out lanes would be used.
 */
#define LANE_JUMP (64 / sizeof(uint64_t))

/*
 * Number of times the algorithm will try to reacquire the primary lane for the
 * thread. If this threshold is exceeded, a new primary lane is selected for the
 * thread.
 */
#define LANE_PRIMARY_ATTEMPTS 128

#define RLANE_DEFAULT 0

enum lane_section_type {
	LANE_SECTION_ALLOCATOR,
	LANE_SECTION_LIST,
	LANE_SECTION_TRANSACTION,

	MAX_LANE_SECTION,

	LANE_ID = MAX_LANE_SECTION
};

struct lane_section_layout {
	unsigned char data[LANE_SECTION_LEN];
};

struct lane_section {
	/* persistent */
	struct lane_section_layout *layout;

	void *runtime;
};

struct lane_layout {
	struct lane_section_layout sections[MAX_LANE_SECTION];
};

struct lane {
	/* volatile state */
	struct lane_section sections[MAX_LANE_SECTION];
};

struct lane_descriptor {
	/*
	 * Number of lanes available at runtime must be <= total number of lanes
	 * available in the pool. Number of lanes can be limited by shortage of
	 * other resources e.g. available RNIC's submission queue sizes.
	 */
	unsigned runtime_nlanes;
	unsigned next_lane_idx;
	uint64_t *lane_locks;
	struct lane *lane;
};

typedef int (*section_layout_op)(PMEMobjpool *pop, void *data, unsigned length);
typedef void *(*section_constr)(PMEMobjpool *pop);
typedef void (*section_destr)(PMEMobjpool *pop, void *rt);
typedef int (*section_global_op)(PMEMobjpool *pop);

struct section_operations {
	section_constr construct_rt;
	section_destr destroy_rt;
	section_layout_op check;
	section_layout_op recover;
	section_global_op boot;
};

struct lane_info {
	uint64_t pop_uuid_lo;
	uint64_t lane_idx;
	unsigned long nest_count;

	/*
	 * The index of the primary lane for the thread. A thread will always
	 * try to acquire the primary lane first, and only if that fails it will
	 * look for a different available lane.
	 */
	uint64_t primary;
	int primary_attempts;

	struct lane_info *prev, *next;
};

extern struct section_operations *Section_ops[MAX_LANE_SECTION];

void lane_info_boot(void);
void lane_info_destroy(void);

int lane_boot(PMEMobjpool *pop);
void lane_cleanup(PMEMobjpool *pop);
int lane_recover_and_section_boot(PMEMobjpool *pop);
int lane_check(PMEMobjpool *pop);

unsigned lane_hold(PMEMobjpool *pop, struct lane_section **section,
	enum lane_section_type type);
void lane_release(PMEMobjpool *pop);

#ifndef _MSC_VER

#define SECTION_PARM(n, ops)\
__attribute__((constructor)) static void _section_parm_##n(void)\
{ Section_ops[n] = ops; }

#else

#define SECTION_PARM(n, ops)\
static void _section_parm_##n(void)\
{ Section_ops[n] = ops; }\
MSVC_CONSTR(_section_parm_##n)

#endif

#endif
