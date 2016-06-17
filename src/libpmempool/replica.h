/*
 * Copyright 2016, Intel Corporation
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
#include "obj.h"

#define UNDEF_REPLICA UINT_MAX
#define UNDEF_PART UINT_MAX
#define IS_BROKEN (1 << 0)
#define IS_INCONSISTENT (1 << 1)

/*
 * Helping structures for storing replica and poolset's health status
 */
struct replica_health_status {
	unsigned nparts;
	unsigned flags;
	unsigned part[];
};

struct poolset_health_status {
	unsigned nreplicas;
	unsigned flags;
	struct replica_health_status *replica[];
};

size_t replica_get_part_data_len(struct pool_set *set_in, unsigned repn,
		unsigned partn);
size_t replica_get_part_range_data_len(struct pool_set *set_in, unsigned repn,
		unsigned pstart, unsigned pend);
uint64_t replica_get_part_data_offset(struct pool_set *set_in, unsigned repn,
		unsigned part);

/*
 * is_dry_run -- (internal) check whether only verification mode is enabled
 */
static inline bool
is_dry_run(unsigned flags)
{
	return PMEMPOOL_DRY_RUN & flags;
}

int replica_remove_part(struct pool_set *set, unsigned repn, unsigned partn);
int create_poolset_health_status(struct pool_set *set,
		struct poolset_health_status **set_hsp);
void free_poolset_health_status(struct poolset_health_status *set_s);
int check_poolset_health(struct pool_set *set,
		struct poolset_health_status **set_hs,
		unsigned flags);
int is_part_broken(unsigned repn, unsigned partn,
		struct poolset_health_status *set_hs);
int is_replica_broken(unsigned repn, struct poolset_health_status *set_hs);
int is_poolset_healthy(struct poolset_health_status *set_hs);
int is_replica_inconsistent(unsigned repn,
		struct poolset_health_status *set_hs);
unsigned find_healthy_replica(struct poolset_health_status *set_hs);
size_t get_pool_size(struct pool_set *set);


int open_replica_part_files(struct pool_set *set, unsigned repn);
int open_poolset_part_files(struct pool_set *set);

int sync_replica(struct pool_set *set_in, unsigned flags);
int transform_replica(struct pool_set *set_in, struct pool_set *set_out,
		unsigned flags);
