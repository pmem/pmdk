/*
 * Copyright 2016-2019, Intel Corporation
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
 * replica.h -- module for synchronizing and transforming poolset
 */
#ifndef REPLICA_H
#define REPLICA_H

#include "libpmempool.h"
#include "pool.h"
#include "os_badblock.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UNDEF_REPLICA UINT_MAX
#define UNDEF_PART UINT_MAX

/*
 * A part marked as broken does not exist or is damaged so that
 * it cannot be opened and has to be recreated.
 */
#define IS_BROKEN		(1U << 0)

/*
 * A replica marked as inconsistent exists but has inconsistent metadata
 * (e.g. inconsistent parts or replicas linkage)
 */
#define IS_INCONSISTENT		(1U << 1)

/*
 * A part or replica marked in this way has bad blocks inside.
 */
#define HAS_BAD_BLOCKS		(1U << 2)

/*
 * A part marked in this way has bad blocks in the header
 */
#define HAS_CORRUPTED_HEADER	(1U << 3)

/*
 * A flag which can be passed to sync_replica() to indicate that the function is
 * called by pmempool_transform
 */
#define IS_TRANSFORMED		(1U << 10)

/*
 * Number of lanes utilized when working with remote replicas
 */
#define REMOTE_NLANES	1

/*
 * Helping structures for storing part's health status
 */
struct part_health_status {
	unsigned flags;
	struct badblocks bbs;		/* structure with bad blocks */
	char *recovery_file_name;	/* name of bad block recovery file */
	int recovery_file_exists;	/* bad block recovery file exists */
};

/*
 * Helping structures for storing replica and poolset's health status
 */
struct replica_health_status {
	unsigned nparts;
	unsigned nhdrs;
	/* a flag for the replica */
	unsigned flags;
	/* effective size of a pool, valid only for healthy replica */
	size_t pool_size;
	/* flags for each part */
	struct part_health_status part[];
};

struct poolset_health_status {
	unsigned nreplicas;
	/* a flag for the poolset */
	unsigned flags;
	/* health statuses for each replica */
	struct replica_health_status *replica[];
};

/* get index of the (r)th replica health status */
static inline unsigned
REP_HEALTHidx(struct poolset_health_status *set, unsigned r)
{
	ASSERTne(set->nreplicas, 0);
	return (set->nreplicas + r) % set->nreplicas;
}

/* get index of the (r + 1)th replica health status */
static inline unsigned
REPN_HEALTHidx(struct poolset_health_status *set, unsigned r)
{
	ASSERTne(set->nreplicas, 0);
	return (set->nreplicas + r + 1) % set->nreplicas;
}

/* get (p)th part health status */
static inline unsigned
PART_HEALTHidx(struct replica_health_status *rep, unsigned p)
{
	ASSERTne(rep->nparts, 0);
	return (rep->nparts + p) % rep->nparts;
}

/* get (r)th replica health status */
static inline struct replica_health_status *
REP_HEALTH(struct poolset_health_status *set, unsigned r)
{
	return set->replica[REP_HEALTHidx(set, r)];
}

/* get (p)th part health status */
static inline unsigned
PART_HEALTH(struct replica_health_status *rep, unsigned p)
{
	return rep->part[PART_HEALTHidx(rep, p)].flags;
}

uint64_t replica_get_part_offset(struct pool_set *set,
		unsigned repn, unsigned partn);

void replica_align_badblock_offset_length(size_t *offset, size_t *length,
		struct pool_set *set_in, unsigned repn, unsigned partn);

size_t replica_get_part_data_len(struct pool_set *set_in, unsigned repn,
		unsigned partn);
uint64_t replica_get_part_data_offset(struct pool_set *set_in, unsigned repn,
		unsigned part);

/*
 * is_dry_run -- (internal) check whether only verification mode is enabled
 */
static inline bool
is_dry_run(unsigned flags)
{
	/*
	 * PMEMPOOL_SYNC_DRY_RUN and PMEMPOOL_TRANSFORM_DRY_RUN
	 * have to have the same value in order to use this common function.
	 */
	ASSERT_COMPILE_ERROR_ON(PMEMPOOL_SYNC_DRY_RUN !=
				PMEMPOOL_TRANSFORM_DRY_RUN);

	return flags & PMEMPOOL_SYNC_DRY_RUN;
}

/*
 * fix_bad_blocks -- (internal) fix bad blocks - it causes reading or creating
 *                              bad blocks recovery files
 *                              (depending on if they exist or not)
 */
static inline bool
fix_bad_blocks(unsigned flags)
{
	return flags & PMEMPOOL_SYNC_FIX_BAD_BLOCKS;
}

int replica_remove_all_recovery_files(struct poolset_health_status *set_hs);
int replica_remove_part(struct pool_set *set, unsigned repn, unsigned partn,
		int fix_bad_blocks);
int replica_create_poolset_health_status(struct pool_set *set,
		struct poolset_health_status **set_hsp);
void replica_free_poolset_health_status(struct poolset_health_status *set_s);
int replica_check_poolset_health(struct pool_set *set,
		struct poolset_health_status **set_hs,
		int called_from_sync, unsigned flags);
int replica_is_part_broken(unsigned repn, unsigned partn,
		struct poolset_health_status *set_hs);
int replica_has_bad_blocks(unsigned repn, struct poolset_health_status *set_hs);
int replica_part_has_bad_blocks(struct part_health_status *phs);
int replica_part_has_corrupted_header(unsigned repn, unsigned partn,
				struct poolset_health_status *set_hs);
unsigned replica_find_unbroken_part(unsigned repn,
		struct poolset_health_status *set_hs);
int replica_is_replica_broken(unsigned repn,
		struct poolset_health_status *set_hs);
int replica_is_replica_consistent(unsigned repn,
		struct poolset_health_status *set_hs);
int replica_is_replica_healthy(unsigned repn,
		struct poolset_health_status *set_hs);

unsigned replica_find_healthy_replica(
		struct poolset_health_status *set_hs);
unsigned replica_find_replica_healthy_header(
		struct poolset_health_status *set_hs);

int replica_is_poolset_healthy(struct poolset_health_status *set_hs);
int replica_is_poolset_transformed(unsigned flags);
ssize_t replica_get_pool_size(struct pool_set *set, unsigned repn);
int replica_check_part_sizes(struct pool_set *set, size_t min_size);
int replica_check_part_dirs(struct pool_set *set);
int replica_check_local_part_dir(struct pool_set *set, unsigned repn,
		unsigned partn);

int replica_open_replica_part_files(struct pool_set *set, unsigned repn);
int replica_open_poolset_part_files(struct pool_set *set);

int replica_sync(struct pool_set *set_in, struct poolset_health_status *set_hs,
		unsigned flags);
int replica_transform(struct pool_set *set_in, struct pool_set *set_out,
		unsigned flags);

#ifdef __cplusplus
}
#endif

#endif
