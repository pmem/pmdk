/*
 * Copyright (c) 2015-2016, Intel Corporation
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
 * lane.c -- lane implementation
 */

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE
#endif

#include <errno.h>

#include "libpmemobj.h"
#include "lane.h"
#include "util.h"
#include "out.h"
#include "redo.h"
#include "list.h"
#include "sys_util.h"
#include "obj.h"
#include "valgrind_internal.h"

__thread unsigned Lane_idx = UINT32_MAX;
static unsigned Next_lane_idx = 0;

struct section_operations *Section_ops[MAX_LANE_SECTION];

/*
 * lane_get_layout -- (internal) calculates the real pointer of the lane layout
 */
static struct lane_layout *
lane_get_layout(PMEMobjpool *pop, uint64_t lane_idx)
{
	return (void *)((char *)pop + pop->lanes_offset +
		sizeof (struct lane_layout) * lane_idx);
}

/*
 * lane_init -- (internal) initializes a single lane runtime variables
 */
static int
lane_init(PMEMobjpool *pop, struct lane *lane, struct lane_layout *layout,
		pthread_mutex_t *mtx, pthread_mutexattr_t *attr)
{
	ASSERTne(lane, NULL);
	ASSERTne(mtx, NULL);
	ASSERTne(attr, NULL);

	int err;

	util_mutex_init(mtx, attr);

	lane->lock = mtx;

	int i;
	for (i = 0; i < MAX_LANE_SECTION; ++i) {
		lane->sections[i].runtime = NULL;
		lane->sections[i].layout = &layout->sections[i];
		err = Section_ops[i]->construct(pop, &lane->sections[i]);
		if (err != 0) {
			ERR("!lane_construct_ops %d", i);
			goto error_section_construct;
		}
	}

	return 0;

error_section_construct:
	for (i = i - 1; i >= 0; --i)
		Section_ops[i]->destruct(pop, &lane->sections[i]);

	util_mutex_destroy(lane->lock);
	return err;
}

/*
 * lane_destroy -- cleanups a single lane runtime variables
 */
static void
lane_destroy(PMEMobjpool *pop, struct lane *lane)
{
	for (int i = 0; i < MAX_LANE_SECTION; ++i)
		Section_ops[i]->destruct(pop, &lane->sections[i]);

	util_mutex_destroy(lane->lock);
}

/*
 * lane_boot -- initializes all lanes
 */
int
lane_boot(PMEMobjpool *pop)
{
	ASSERTeq(pop->lanes, NULL);

	int err;

	pthread_mutexattr_t lock_attr;
	if ((err = pthread_mutexattr_init(&lock_attr)) != 0) {
		ERR("!pthread_mutexattr_init");
		goto error_lanes_malloc;
	}

	if ((err = pthread_mutexattr_settype(
			&lock_attr, PTHREAD_MUTEX_RECURSIVE)) != 0) {
		ERR("!pthread_mutexattr_settype");
		goto error_lanes_malloc;
	}

	pop->lanes = Malloc(sizeof (struct lane) * pop->nlanes);
	if (pop->lanes == NULL) {
		err = ENOMEM;
		ERR("!Malloc of volatile lanes");
		goto error_lanes_malloc;
	}

	pop->lane_locks = Malloc(sizeof (pthread_mutex_t) * pop->nlanes);
	if (pop->lane_locks == NULL) {
		err = ENOMEM;
		ERR("!Malloc for lane locks");
		goto error_lock_malloc;
	}

	/* add lanes to pmemcheck ignored list */
	VALGRIND_ADD_TO_GLOBAL_TX_IGNORE((char *)pop + pop->lanes_offset,
		(sizeof (struct lane_layout) * pop->nlanes));

	uint64_t i;
	for (i = 0; i < pop->nlanes; ++i) {
		struct lane_layout *layout = lane_get_layout(pop, i);

		if ((err = lane_init(pop, &pop->lanes[i], layout,
				&pop->lane_locks[i], &lock_attr)) != 0) {
			ERR("!lane_init");
			goto error_lane_init;
		}
	}

	if (pthread_mutexattr_destroy(&lock_attr) != 0) {
		ERR("!pthread_mutexattr_destroy");
		goto error_mutexattr_destroy;
	}

	return 0;

error_lane_init:
	for (; i >= 1; --i)
		lane_destroy(pop, &pop->lanes[i - 1]);

	Free(pop->lane_locks);
	pop->lane_locks = NULL;

error_lock_malloc:
	Free(pop->lanes);
	pop->lanes = NULL;

error_lanes_malloc:
	if (pthread_mutexattr_destroy(&lock_attr) != 0)
		ERR("!pthread_mutexattr_destroy");

error_mutexattr_destroy:
	return err;
}

/*
 * lane_cleanup -- destroys all lanes
 */
void
lane_cleanup(PMEMobjpool *pop)
{
	ASSERTne(pop->lanes, NULL);

	for (uint64_t i = 0; i < pop->nlanes; ++i)
		lane_destroy(pop, &pop->lanes[i]);

	Free(pop->lane_locks);
	pop->lane_locks = NULL;

	Free(pop->lanes);
	pop->lanes = NULL;
}

/*
 * lane_recover_and_boot -- performs initialization and recovery of all lanes
 */
int
lane_recover_and_section_boot(PMEMobjpool *pop)
{
	int err = 0;
	int i; /* section index */
	uint64_t j; /* lane index */
	struct lane_layout *layout;

	for (i = 0; i < MAX_LANE_SECTION; ++i) {
		for (j = 0; j < pop->nlanes; ++j) {
			layout = lane_get_layout(pop, j);
			err = Section_ops[i]->recover(pop,
				&layout->sections[i]);

			if (err != 0) {
				LOG(2, "section_ops->recover %d %ju %d",
					i, j, err);
				return err;
			}
		}

		if ((err = Section_ops[i]->boot(pop)) != 0) {
			LOG(2, "section_ops->init %d %d", i, err);
			return err;
		}
	}

	return err;
}

/*
 * lane_check -- performs check of all lanes
 */
int
lane_check(PMEMobjpool *pop)
{
	int err = 0;
	int i; /* section index */
	uint64_t j; /* lane index */
	struct lane_layout *layout;

	for (i = 0; i < MAX_LANE_SECTION; ++i) {
		for (j = 0; j < pop->nlanes; ++j) {
			layout = lane_get_layout(pop, j);
			err = Section_ops[i]->check(pop, &layout->sections[i]);

			if (err) {
				LOG(2, "section_ops->check %d %ju %d",
					i, j, err);

				return err;
			}
		}
	}

	return err;
}

/*
 * lane_hold -- grabs a per-thread lane in a round-robin fashion
 */
void
lane_hold(PMEMobjpool *pop, struct lane_section **section,
	enum lane_section_type type)
{
	ASSERTne(section, NULL);
	ASSERTne(pop->lanes, NULL);

	if (Lane_idx == UINT32_MAX) {
		do {
			Lane_idx = __sync_fetch_and_add(&Next_lane_idx, 1);
		} while (Lane_idx == UINT32_MAX); /* handles wraparound */
	}

	struct lane *lane = &pop->lanes[Lane_idx % pop->nlanes];

	util_mutex_lock(lane->lock);

	*section = &lane->sections[type];
}

/*
 * lane_release -- drops the per-thread lane
 */
void
lane_release(PMEMobjpool *pop)
{
	ASSERT(Lane_idx != UINT32_MAX);
	ASSERTne(pop->lanes, NULL);

	struct lane *lane = &pop->lanes[Lane_idx % pop->nlanes];

	util_mutex_unlock(lane->lock);
}
