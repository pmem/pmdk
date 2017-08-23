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
 * lane.c -- lane implementation
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <inttypes.h>
#include <errno.h>
#include <limits.h>
#include <sched.h>

#include "libpmemobj.h"
#include "cuckoo.h"
#include "lane.h"
#include "out.h"
#include "util.h"
#include "obj.h"
#include "os_thread.h"
#include "valgrind_internal.h"

static os_tls_key_t Lane_info_key;

static __thread struct cuckoo *Lane_info_ht;
static __thread struct lane_info *Lane_info_records;
static __thread struct lane_info *Lane_info_cache;

struct section_operations *Section_ops[MAX_LANE_SECTION];

/*
 * lane_info_destroy -- destroy lane info hash table
 */
inline void
lane_info_destroy(void)
{
	if (unlikely(Lane_info_ht == NULL))
		return;

	cuckoo_delete(Lane_info_ht);
	struct lane_info *record;
	struct lane_info *head = Lane_info_records;
	while (head != NULL) {
		record = head;
		head = head->next;
		Free(record);
	}

	Lane_info_ht = NULL;
	Lane_info_records = NULL;
	Lane_info_cache = NULL;
}

/*
 * lane_info_ht_destroy -- (internal) destructor for thread shared data
 */
static inline void
lane_info_ht_destroy(void *ht)
{
	lane_info_destroy();
}

/*
 * lane_info_create -- (internal) constructor for thread shared data
 */
static inline void
lane_info_create(void)
{
	Lane_info_ht = cuckoo_new();
	if (Lane_info_ht == NULL)
		FATAL("cuckoo_new");
}

/*
 * lane_info_boot -- initialize lane info hash table and lane info key
 */
void
lane_info_boot(void)
{
	int result = os_tls_key_create(&Lane_info_key, lane_info_ht_destroy);
	if (result != 0) {
		errno = result;
		FATAL("!os_tls_key_create");
	}
}

/*
 * lane_info_ht_boot -- (internal) boot lane info and add it to thread shared
 *	data
 */
static inline void
lane_info_ht_boot(void)
{
	lane_info_create();
	int result = os_tls_set(Lane_info_key, Lane_info_ht);
	if (result != 0) {
		errno = result;
		FATAL("!os_tls_set");
	}
}

/*
 * lane_info_cleanup -- remove lane info record regarding pool being deleted
 */
static inline void
lane_info_cleanup(PMEMobjpool *pop)
{
	if (unlikely(Lane_info_ht == NULL))
		return;

	struct lane_info *info = cuckoo_remove(Lane_info_ht, pop->uuid_lo);
	if (likely(info != NULL)) {
		if (info->prev)
			info->prev->next = info->next;

		if (info->next)
			info->next->prev = info->prev;

		if (Lane_info_cache == info)
			Lane_info_cache = NULL;

		if (Lane_info_records == info)
			Lane_info_records = info->next;

		Free(info);
	}

	(void) os_tls_key_delete(Lane_info_key);
}

/*
 * lane_get_layout -- (internal) calculates the real pointer of the lane layout
 */
static struct lane_layout *
lane_get_layout(PMEMobjpool *pop, uint64_t lane_idx)
{
	return (void *)((char *)pop + pop->lanes_offset +
		sizeof(struct lane_layout) * lane_idx);
}

/*
 * lane_init -- (internal) initializes a single lane runtime variables
 */
static int
lane_init(PMEMobjpool *pop, struct lane *lane, struct lane_layout *layout)
{
	ASSERTne(lane, NULL);

	int i;
	int oerrno;

	for (i = 0; i < MAX_LANE_SECTION; ++i) {
		lane->sections[i].layout = &layout->sections[i];
		errno = 0;
		lane->sections[i].runtime = Section_ops[i]->construct_rt(pop);
		if (lane->sections[i].runtime == NULL && errno) {
			ERR("!lane_construct_ops %d", i);
			goto error_section_construct;
		}
	}
	return 0;

error_section_construct:
	oerrno = errno;
	for (i = i - 1; i >= 0; --i)
		Section_ops[i]->destroy_rt(pop, &lane->sections[i].runtime);
	errno = oerrno;
	return -1;
}

/*
 * lane_destroy -- cleanups a single lane runtime variables
 */
static void
lane_destroy(PMEMobjpool *pop, struct lane *lane)
{
	for (int i = 0; i < MAX_LANE_SECTION; ++i)
		Section_ops[i]->destroy_rt(pop, lane->sections[i].runtime);
}

/*
 * lane_boot -- initializes all lanes
 */
int
lane_boot(PMEMobjpool *pop)
{
	int err = 0;

	pop->lanes_desc.lane = Malloc(sizeof(struct lane) * pop->nlanes);
	if (pop->lanes_desc.lane == NULL) {
		err = ENOMEM;
		ERR("!Malloc of volatile lanes");
		goto error_lanes_malloc;
	}

	pop->lanes_desc.next_lane_idx = 0;

	pop->lanes_desc.lane_locks =
		Zalloc(sizeof(*pop->lanes_desc.lane_locks) * pop->nlanes);
	if (pop->lanes_desc.lane_locks == NULL) {
		ERR("!Malloc for lane locks");
		goto error_locks_malloc;
	}

	/* add lanes to pmemcheck ignored list */
	VALGRIND_ADD_TO_GLOBAL_TX_IGNORE((char *)pop + pop->lanes_offset,
		(sizeof(struct lane_layout) * pop->nlanes));

	uint64_t i;
	for (i = 0; i < pop->nlanes; ++i) {
		struct lane_layout *layout = lane_get_layout(pop, i);

		if ((err = lane_init(pop, &pop->lanes_desc.lane[i], layout))) {
			ERR("!lane_init");
			goto error_lane_init;
		}
	}

	return 0;

error_lane_init:
	for (; i >= 1; --i)
		lane_destroy(pop, &pop->lanes_desc.lane[i - 1]);
	Free(pop->lanes_desc.lane_locks);
	pop->lanes_desc.lane_locks = NULL;
error_locks_malloc:
	Free(pop->lanes_desc.lane);
	pop->lanes_desc.lane = NULL;
error_lanes_malloc:
	return err;
}

/*
 * lane_cleanup -- destroys all lanes
 */
void
lane_cleanup(PMEMobjpool *pop)
{
	for (uint64_t i = 0; i < pop->nlanes; ++i)
		lane_destroy(pop, &pop->lanes_desc.lane[i]);

	Free(pop->lanes_desc.lane);
	pop->lanes_desc.lane = NULL;
	Free(pop->lanes_desc.lane_locks);
	pop->lanes_desc.lane_locks = NULL;

	lane_info_cleanup(pop);
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
			err = Section_ops[i]->recover(pop, &layout->sections[i],
				sizeof(layout->sections[i]));

			if (err != 0) {
				LOG(2, "section_ops->recover %d %" PRIu64 " %d",
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
			err = Section_ops[i]->check(pop, &layout->sections[i],
					sizeof(layout->sections[i]));

			if (err) {
				LOG(2, "section_ops->check %d %" PRIu64 " %d",
					i, j, err);

				return err;
			}
		}
	}

	return err;
}

/*
 * get_lane -- (internal) get free lane index
 */
static inline void
get_lane(uint64_t *locks, struct lane_info *info, uint64_t nlocks)
{
	info->lane_idx = info->primary;
	while (1) {
		do {
			info->lane_idx %= nlocks;
			if (likely(util_bool_compare_and_swap64(
					&locks[info->lane_idx], 0, 1))) {
				if (info->lane_idx == info->primary) {
					info->primary_attempts =
						LANE_PRIMARY_ATTEMPTS;
				} else if (info->primary_attempts == 0) {
					info->primary = info->lane_idx;
					info->primary_attempts =
						LANE_PRIMARY_ATTEMPTS;
				}
				return;
			}

			if (info->lane_idx == info->primary &&
					info->primary_attempts > 0) {
				info->primary_attempts--;
			}

			++info->lane_idx;
		} while (info->lane_idx < nlocks);

		sched_yield();
	}
}

/*
 * get_lane_info_record -- (internal) get lane record attached to memory pool
 *	or first free
 */
static inline struct lane_info *
get_lane_info_record(PMEMobjpool *pop)
{
	if (likely(Lane_info_cache != NULL &&
			Lane_info_cache->pop_uuid_lo == pop->uuid_lo)) {
		return Lane_info_cache;
	}

	if (unlikely(Lane_info_ht == NULL)) {
		lane_info_ht_boot();
	}

	struct lane_info *info = cuckoo_get(Lane_info_ht, pop->uuid_lo);

	if (unlikely(info == NULL)) {
		info = Malloc(sizeof(struct lane_info));
		if (unlikely(info == NULL)) {
			FATAL("Malloc");
		}
		info->pop_uuid_lo = pop->uuid_lo;
		info->lane_idx = UINT64_MAX;
		info->nest_count = 0;
		info->next = Lane_info_records;
		info->prev = NULL;
		info->primary = 0;
		info->primary_attempts = LANE_PRIMARY_ATTEMPTS;
		if (Lane_info_records) {
			Lane_info_records->prev = info;
		}
		Lane_info_records = info;

		if (unlikely(cuckoo_insert(
				Lane_info_ht, pop->uuid_lo, info) != 0)) {
			FATAL("cuckoo_insert");
		}
	}

	Lane_info_cache = info;
	return info;
}

/*
 * lane_hold -- grabs a per-thread lane in a round-robin fashion
 */
unsigned
lane_hold(PMEMobjpool *pop, struct lane_section **section,
	enum lane_section_type type)
{
	if (section == NULL)
		ASSERTeq(type, LANE_ID);

	/*
	 * Before runtime lane initialization all remote operations are
	 * executed using RLANE_DEFAULT.
	 */
	if (unlikely(!pop->lanes_desc.runtime_nlanes)) {
		ASSERT(pop->has_remote_replicas);
		if (section != NULL)
			FATAL("cannot obtain section before lane's init");
		return RLANE_DEFAULT;
	}

	struct lane_info *lane = get_lane_info_record(pop);
	while (unlikely(lane->lane_idx == UINT64_MAX)) {
		/* initial wrap to next CL */
		lane->primary = lane->lane_idx = __sync_fetch_and_add(
			&pop->lanes_desc.next_lane_idx, LANE_JUMP);
	} /* handles wraparound */

	uint64_t *llocks = pop->lanes_desc.lane_locks;
	/* grab next free lane from lanes available at runtime */
	if (!lane->nest_count++) {
		get_lane(llocks, lane, pop->lanes_desc.runtime_nlanes);
	}

	if (section) {
		ASSERT(type < MAX_LANE_SECTION);
		struct lane_section *s =
			&pop->lanes_desc.lane[lane->lane_idx].sections[type];

		VALGRIND_ANNOTATE_NEW_MEMORY(s, sizeof(*s));
		VALGRIND_ANNOTATE_NEW_MEMORY(s->layout, sizeof(*s->layout));

		*section = s;
	}

	return (unsigned)lane->lane_idx;
}

/*
 * lane_attach -- attaches the lane with the given index to the current thread
 */
void
lane_attach(PMEMobjpool *pop, unsigned lane)
{
	struct lane_info *info = get_lane_info_record(pop);
	info->nest_count = 1;
	info->lane_idx = lane;
}

/*
 * lane_detach -- detaches the currently held lane from the current thread
 */
unsigned
lane_detach(PMEMobjpool *pop)
{
	struct lane_info *lane = get_lane_info_record(pop);
	lane->nest_count -= 1;
	ASSERTeq(lane->nest_count, 0);

	return (unsigned)lane->lane_idx;
}

/*
 * lane_release -- drops the per-thread lane
 */
void
lane_release(PMEMobjpool *pop)
{
	if (unlikely(!pop->lanes_desc.runtime_nlanes)) {
		ASSERT(pop->has_remote_replicas);
		return;
	}

	struct lane_info *lane = get_lane_info_record(pop);

	ASSERTne(lane, NULL);
	ASSERTne(lane->lane_idx, UINT64_MAX);

	if (unlikely(lane->nest_count == 0)) {
		FATAL("lane_release");
	} else if (--(lane->nest_count) == 0) {
		if (unlikely(!util_bool_compare_and_swap64(
				&pop->lanes_desc.lane_locks[lane->lane_idx],
				1, 0))) {
			FATAL("util_bool_compare_and_swap64");
		}
	}
}
