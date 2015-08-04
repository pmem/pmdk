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
 * lane.c -- lane implementation
 */

#ifndef	_GNU_SOURCE
#define	_GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>
#include <pthread.h>

#include "libpmemobj.h"
#include "lane.h"
#include "util.h"
#include "out.h"
#include "redo.h"
#include "list.h"
#include "obj.h"
#include "valgrind_internal.h"

static __thread int lane_idx = -1;
static int next_lane_idx = 0;

struct section_operations *section_ops[MAX_LANE_SECTION];

/*
 * lane_get_layout -- (internal) calculates the real pointer of the lane layout
 */
static struct lane_layout *
lane_get_layout(PMEMobjpool *pop, int lane_idx)
{
	return (void *)pop + pop->lanes_offset +
		(sizeof (struct lane_layout) * lane_idx);
}

/*
 * lane_init -- (internal) initializes a single lane runtime variables
 */
static int
lane_init(struct lane *lane, struct lane_layout *layout)
{
	ASSERTne(lane, NULL);

	int err = 0;

	lane->lock = Malloc(sizeof (*lane->lock));
	if (lane->lock == NULL) {
		err = ENOMEM;
		ERR("!Malloc for lane lock");
		goto error_lock_malloc;
	}

	pthread_mutexattr_t lock_attr;
	if ((err = pthread_mutexattr_init(&lock_attr)) != 0) {
		ERR("!pthread_mutexattr_init");
		goto error_lock_attr_init;
	}

	if ((err = pthread_mutexattr_settype(
		&lock_attr, PTHREAD_MUTEX_RECURSIVE)) != 0) {
		ERR("!pthread_mutexattr_settype");
		goto error_lock_attr_set;
	}

	if ((err = pthread_mutex_init(lane->lock, &lock_attr)) != 0) {
		ERR("!pthread_mutex_init");
		goto error_lock_init;
	}

	if ((err = pthread_mutexattr_destroy(&lock_attr)) != 0) {
		ERR("!pthread_mutexattr_destroy");
		goto error_lock_attr_destroy;
	}

	int i;
	for (i = 0; i < MAX_LANE_SECTION; ++i) {
		lane->sections[i].runtime = NULL;
		lane->sections[i].layout = &layout->sections[i];
		if ((err =
			section_ops[i]->construct(&lane->sections[i])) != 0) {
			ERR("!lane_construct_ops %d", i);
			goto error_section_construct;
		}
	}

	return 0;

error_section_construct:
	for (i = i - 1; i >= 0; --i)
		if (section_ops[i]->destruct(&lane->sections[i]) != 0)
			ERR("!lane_destruct_ops %d", i);
error_lock_attr_destroy:
	if (pthread_mutex_destroy(lane->lock) != 0)
		ERR("!pthread_mutex_destroy");
error_lock_init:
error_lock_attr_set:
	if (pthread_mutexattr_destroy(&lock_attr) != 0)
		ERR("!pthread_mutexattr_destroy");
error_lock_attr_init:
	Free(lane->lock);
error_lock_malloc:
	return err;
}

/*
 * lane_destroy -- cleanups a single lane runtime variables
 */
static int
lane_destroy(struct lane *lane)
{
	int err = 0;

	for (int i = 0; i < MAX_LANE_SECTION; ++i)
		if ((err = section_ops[i]->destruct(&lane->sections[i])) != 0)
			ERR("!lane_destruct_ops %d", i);

	if ((err = pthread_mutex_destroy(lane->lock)) != 0)
		ERR("!pthread_mutex_destroy");

	Free(lane->lock);

	return err;
}

/*
 * lane_boot -- initializes all lanes
 */
int
lane_boot(PMEMobjpool *pop)
{
	ASSERTeq(pop->lanes, NULL);

	int err = 0;

	pop->lanes = Malloc(sizeof (struct lane) * pop->nlanes);

	if (pop->lanes == NULL) {
		ERR("!Malloc of volatile lanes");
		err = ENOMEM;
		goto error_lanes_malloc;
	}

	/* add lanes to pmemcheck ignored list */
	VALGRIND_ADD_TO_GLOBAL_TX_IGNORE((void *)pop + pop->lanes_offset,
		(sizeof (struct lane_layout) * pop->nlanes));
	int i;
	for (i = 0; i < pop->nlanes; ++i) {
		struct lane_layout *layout = lane_get_layout(pop, i);

		if ((err = lane_init(&pop->lanes[i], layout)) != 0) {
			ERR("!lane_init");
			goto error_lane_init;
		}
	}

	return 0;

error_lane_init:
	for (i = i - 1; i >= 0; --i)
		if (lane_destroy(&pop->lanes[i]) != 0)
			ERR("!lane_destroy");
	Free(pop->lanes);
	pop->lanes = NULL;
error_lanes_malloc:
	return err;
}

/*
 * lane_cleanup -- destroys all lanes
 */
int
lane_cleanup(PMEMobjpool *pop)
{
	ASSERTne(pop->lanes, NULL);

	int err = 0;

	for (int i = 0; i < pop->nlanes; ++i)
		if ((err = lane_destroy(&pop->lanes[i])) != 0)
			ERR("!lane_destroy");

	Free(pop->lanes);
	pop->lanes = NULL;

	return err;
}

#define	FOREACH_LANE_SECTION(pop, var, lane, section)\
for (section = 0; section < MAX_LANE_SECTION; ++section)\
	for (lane = 0, var = lane_get_layout(pop, lane);\
		lane < pop->nlanes; ++lane)

/*
 * lane_recover -- performs recovery of all lanes
 */
int
lane_recover(PMEMobjpool *pop)
{
	int err = 0;
	int section_err;
	int i; /* lane index */
	int j; /* section index */
	struct lane_layout *layout;

	FOREACH_LANE_SECTION(pop, layout, i, j) {
		section_err = section_ops[j]->recover(pop,
				&layout->sections[j]);
		if (section_err) {
			ERR("!section_ops->recover %d %d %d",
					i, j, section_err);
			err = section_err;
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
	int section_err;
	int i; /* lane index */
	int j; /* section index */
	struct lane_layout *layout;

	FOREACH_LANE_SECTION(pop, layout, i, j) {
		section_err = section_ops[j]->check(pop,
				&layout->sections[j]);
		if (section_err) {
			LOG(3, "!section_ops->check %d %d %d",
					i, j, section_err);
			err = section_err;
		}
	}

	return err;
}

/*
 * lane_hold -- grabs a per-thread lane in a round-robin fashion
 */
int
lane_hold(PMEMobjpool *pop, struct lane_section **section,
	enum lane_section_type type)
{
	ASSERTne(section, NULL);
	ASSERTne(pop->lanes, NULL);

	int err = 0;

	if (lane_idx == -1)
		lane_idx = __sync_fetch_and_add(&next_lane_idx, 1);

	struct lane *lane = &pop->lanes[lane_idx % pop->nlanes];

	if ((err = pthread_mutex_lock(lane->lock)) != 0)
		ERR("!pthread_mutex_lock");

	*section = &lane->sections[type];

	return err;
}

/*
 * lane_release -- drops the per-thread lane
 */
int
lane_release(PMEMobjpool *pop)
{
	ASSERT(lane_idx >= 0);
	ASSERTne(pop->lanes, NULL);

	int err = 0;

	struct lane *lane = &pop->lanes[lane_idx % pop->nlanes];

	if ((err = pthread_mutex_unlock(lane->lock)) != 0)
		ERR("!pthread_mutex_unlock");

	return err;
}
