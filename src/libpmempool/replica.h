/*
 * Copyright 2016-2018, Intel Corporation
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
#include "libpmempool.h"
#include "pool.h"

#define UNDEF_REPLICA UINT_MAX
#define UNDEF_PART UINT_MAX

/*
 * A part marked as broken does not exist or is damaged so that
 * it cannot be opened and has to be recreated.
 */
#define IS_BROKEN (1 << 0)

/*
 * A replica marked as inconsistent exists but has inconsistent metadata
 * (e.g. inconsistent parts or replicas linkage)
 */
#define IS_INCONSISTENT (1 << 1)

/*
 * A flag which can be passed to sync_replica() to indicate that the function is
 * called by pmempool_transform
 */
#define IS_TRANSFORMED (1 << 10)

/*
 * Number of lanes utilized when working with remote replicas
 */
#define REMOTE_NLANES	1

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
	unsigned part[];
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
	return rep->part[PART_HEALTHidx(rep, p)];
}

/*
 * Flags used to describe which parts/replicas have to be taken
 * into account when performing an operation.
 */
#define LOCAL	0x1
#define REMOTE	0x2
#define ALL	(LOCAL | REMOTE)

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
	return flags & PMEMPOOL_DRY_RUN;
}

int replica_remove_part(struct pool_set *set, unsigned repn, unsigned partn);
int replica_create_poolset_health_status(struct pool_set *set,
		struct poolset_health_status **set_hsp);
void replica_free_poolset_health_status(struct poolset_health_status *set_s);
int replica_check_poolset_health(struct pool_set *set,
		struct poolset_health_status **set_hs,
		unsigned flags, PMEM_progress_cb progress_cb);
int replica_is_part_broken(unsigned repn, unsigned partn,
		struct poolset_health_status *set_hs);
unsigned replica_find_unbroken_part(unsigned repn,
		struct poolset_health_status *set_hs);
unsigned poolset_count_broken_parts(struct pool_set *set,
		struct poolset_health_status *set_hs, int whence);
int replica_is_replica_broken(unsigned repn,
		struct poolset_health_status *set_hs);
int replica_is_replica_consistent(unsigned repn,
		struct poolset_health_status *set_hs);
int replica_is_replica_healthy(unsigned repn,
		struct poolset_health_status *set_hs);
unsigned replica_find_healthy_replica(struct poolset_health_status *set_hs);
int replica_is_poolset_healthy(struct poolset_health_status *set_hs);
int replica_is_poolset_transformed(unsigned flags);
ssize_t replica_get_pool_size(struct pool_set *set, unsigned repn);
int replica_check_part_sizes(struct pool_set *set, size_t min_size);
int replica_check_part_dirs(struct pool_set *set);
int replica_check_local_part_dir(struct pool_set *set, unsigned repn,
		unsigned partn);

void util_memcpy_persist(int is_pmem, void *to, const void *from,
		size_t size, const char *msg, PMEM_progress_cb progress_cb);
int util_rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length,
		unsigned lane, const char *msg, PMEM_progress_cb progress_cb);

int util_rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length,
		unsigned lane, const char *msg, PMEM_progress_cb progress_cb);
void util_break_progress(PMEM_progress_cb progress_cb);

int replica_open_replica_part_files(struct pool_set *set, unsigned repn);
int replica_open_poolset_part_files(struct pool_set *set);

int replica_sync(struct pool_set *set_in, struct poolset_health_status *set_hs,
		unsigned flags, PMEM_progress_cb progress_cb);
int replica_transform(struct pool_set *set_in, struct pool_set *set_out,
		unsigned flags, PMEM_progress_cb progress_cb);
