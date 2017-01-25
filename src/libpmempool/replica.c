/*
 * Copyright 2016-2017, Intel Corporation
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
 * replica.c -- groups all commands for replica manipulation
 */

#include "replica.h"

#include <errno.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

#include "obj.h"
#include "file.h"
#include "out.h"
#include "pool_hdr.h"
#include "set.h"
#include "util.h"
#include "uuid.h"

/*
 * check_flags_sync -- (internal) check if flags are supported for sync
 */
static int
check_flags_sync(unsigned flags)
{
	flags &= ~(unsigned)PMEMPOOL_DRY_RUN;
	return flags > 0;
}

/*
 * check_flags_transform -- (internal) check if flags are supported for
 *                          transform
 */
static int
check_flags_transform(unsigned flags)
{
	flags &= ~(unsigned)PMEMPOOL_DRY_RUN;
	return flags > 0;
}

/*
 * replica_get_part_data_len -- get data length for given part
 */
size_t
replica_get_part_data_len(struct pool_set *set_in, unsigned repn,
		unsigned partn)
{
	return MMAP_ALIGN_DOWN(
			set_in->replica[repn]->part[partn].filesize) -
			((partn == 0) ? POOL_HDR_SIZE : Mmap_align);
}

/*
 * replica_get_part_range_data_len -- get data length in given range
 */
size_t
replica_get_part_range_data_len(struct pool_set *set, unsigned repn,
		unsigned pstart, unsigned pend)
{
	LOG(3, "set %p, repn %u, pstart %u, pend %u", set, repn, pstart, pend);
	size_t len = 0;
	for (unsigned p = pstart; p < pend; ++p)
		len += replica_get_part_data_len(set, repn, p);

	return len;
}

/*
 * replica_get_part_data_offset -- get data length before given part
 */
uint64_t
replica_get_part_data_offset(struct pool_set *set, unsigned repn,
		unsigned partn)
{
	return replica_get_part_range_data_len(set, repn, 0, partn) +
			POOL_HDR_SIZE;
}

/*
 * replica_remove_part -- unlink part from replica
 */
int
replica_remove_part(struct pool_set *set, unsigned repn, unsigned partn)
{
	LOG(3, "set %p, repn %u, partn %u", set, repn, partn);
	struct pool_set_part *part = &PART(REP(set, repn), partn);
	if (part->fd != -1) {
		close(part->fd);
		part->fd = -1;
	}

	int olderrno = errno;
	if (util_unlink(part->path)) {
		if (errno != ENOENT) {
			ERR("removing part %u from replica %u failed",
					partn, repn);
			return -1;
		}
	}

	errno = olderrno;
	LOG(1, "Removed part %s number %u from replica %u", part->path, partn,
			repn);
	return 0;
}

/*
 * create_replica_health_status -- (internal) create helping structure for
 *                                 storing replica's health status
 */
static struct replica_health_status *
create_replica_health_status(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	unsigned nparts = set->replica[repn]->nparts;
	struct replica_health_status *replica_hs;
	replica_hs = Zalloc(sizeof(struct replica_health_status)
			+ nparts * sizeof(unsigned));
	if (replica_hs == NULL) {
		ERR("!Zalloc for replica health status");
		return NULL;
	}
	replica_hs->nparts = nparts;
	return replica_hs;
}

/*
 * replica_free_poolset_health_status -- free memory allocated for helping
 *                                       structure
 */
void
replica_free_poolset_health_status(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);
	for (unsigned i = 0; i < set_hs->nreplicas; ++i) {
		Free(set_hs->replica[i]);
	}
	Free(set_hs);
}

/*
 * replica_create_poolset_health_status -- create helping structure for storing
 *                                 poolset's health status
 */
int
replica_create_poolset_health_status(struct pool_set *set,
		struct poolset_health_status **set_hsp)
{
	LOG(3, "set %p, set_hsp %p", set, set_hsp);
	unsigned nreplicas = set->nreplicas;
	struct poolset_health_status *set_hs;
	set_hs = Zalloc(sizeof(struct poolset_health_status) +
			nreplicas * sizeof(struct replica_health_status *));
	if (set_hs == NULL) {
		ERR("!Zalloc for poolset health state");
		return -1;
	}
	set_hs->nreplicas = nreplicas;
	for (unsigned i = 0; i < nreplicas; ++i) {
		struct replica_health_status *replica_hs =
				create_replica_health_status(set, i);
		if (replica_hs == NULL) {
			replica_free_poolset_health_status(set_hs);
			return -1;
		}
		set_hs->replica[i] = replica_hs;
	}
	*set_hsp = set_hs;
	return 0;
}

/*
 * replica_is_part_broken -- check if part is marked as broken in the helping
 *                           structure
 */
int
replica_is_part_broken(unsigned repn, unsigned partn,
		struct poolset_health_status *set_hs)
{
	return (REP(set_hs, repn)->flags & IS_BROKEN) ||
			(PART(REP(set_hs, repn), partn) & IS_BROKEN);
}

/*
 * is_replica_broken -- check if any part in the replica is marked as broken
 */
int
replica_is_replica_broken(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);
	struct replica_health_status *r_hs = REP(set_hs, repn);
	if (r_hs->flags & IS_BROKEN)
		return 1;

	for (unsigned p = 0; p < r_hs->nparts; ++p) {
		if (replica_is_part_broken(repn, p, set_hs))
			return 1;
	}
	return 0;
}

/*
 * replica_is_replica_consistent -- check if replica is not marked as
 *                                  inconsistent
 */
int
replica_is_replica_consistent(unsigned repn,
		struct poolset_health_status *set_hs)
{
	return !(REP(set_hs, repn)->flags & IS_INCONSISTENT);
}

/*
 * replica_is_replica_healthy -- check if replica is unbroken and consistent
 */
int
replica_is_replica_healthy(unsigned repn,
		struct poolset_health_status *set_hs)
{
	return !replica_is_replica_broken(repn, set_hs) &&
			replica_is_replica_consistent(repn, set_hs);
}

/*
 * replica_is_poolset_healthy -- check if all replicas in a poolset are not
 *                               marked as broken nor inconsistent in the
 *                               helping structure
 */
int
replica_is_poolset_healthy(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs))
			return 0;
	}
	return 1;
}

/*
 * replica_is_poolset_transformed -- check if the flag indicating a call from
 *                                   pmempool_transform is on
 */
int
replica_is_poolset_transformed(unsigned flags)
{
	return flags & IS_TRANSFORMED;
}


/*
 * find_consistent_replica -- (internal) find a replica number, which is not
 *                            marked as inconsistent in the helping structure
 */
static unsigned
find_consistent_replica(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (replica_is_replica_consistent(r, set_hs))
			return r;
	}
	return UNDEF_REPLICA;
}

/*
 * replica_find_unbroken_part -- find a part number in a given
 * replica, which is not marked as broken in the helping structure
 */
unsigned
replica_find_unbroken_part(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);
	for (unsigned p = 0; p < REP(set_hs, repn)->nparts; ++p) {
		if (!replica_is_part_broken(repn, p, set_hs))
			return p;
	}
	return UNDEF_PART;
}

/*
 * replica_find_healthy_replica -- find a replica number which is a good source
 *                                 of data
 */
unsigned
replica_find_healthy_replica(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);
	if (set_hs->nreplicas == 1) {
		return replica_is_replica_broken(0, set_hs) ? UNDEF_REPLICA : 0;
	} else {
		for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
			if (replica_is_replica_healthy(r, set_hs))
				return r;
		}
		return UNDEF_REPLICA;
	}
}

/*
 * replica_check_store_size -- (internal) store size from pool descriptor for
 * replica
 */
static int
replica_check_store_size(struct pool_set *set,
	struct poolset_health_status *set_hs, unsigned repn)
{
	LOG(3, "set %p, set_hs %p, repn %u", set, set_hs, repn);
	struct pool_replica *rep = set->replica[repn];
	struct pmemobjpool pop;

	if (rep->remote) {
		memcpy(&pop.hdr, rep->part[0].hdr, sizeof(pop.hdr));
		void *descr = (void *)((uintptr_t)&pop + POOL_HDR_SIZE);
		if (Rpmem_read(rep->remote->rpp, descr, 0,
				sizeof(pop) - POOL_HDR_SIZE)) {
			return -1;
		}
	} else {
		/* round up map size to Mmap align size */
		if (util_map_part(&rep->part[0], NULL,
				MMAP_ALIGN_UP(sizeof(pop)), 0, MAP_SHARED, 1)) {
			return -1;
		}

		memcpy(&pop, rep->part[0].addr, sizeof(pop));

		util_unmap_part(&rep->part[0]);
	}

	void *dscp = (void *)((uintptr_t)&pop + sizeof(pop.hdr));

	if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop.checksum, 0)) {
		set_hs->replica[repn]->flags |= IS_BROKEN;
		return 0;
	}

	set_hs->replica[repn]->pool_size = pop.heap_offset + pop.heap_size;

	return 0;
}

/*
 * check_store_all_sizes -- (internal) store sizes from pool descriptor for all
 * healthy replicas
 */
static int
check_store_all_sizes(struct pool_set *set,
	struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs))
			continue;
		if (replica_check_store_size(set, set_hs, r))
			return -1;
	}

	return 0;
}

/*
 * check_and_open_poolset_part_files -- (internal) for each part in a poolset
 * check if the part files are accessible, and if not, mark it as broken
 * in a helping structure; then open the part file
 */
static int
check_and_open_poolset_part_files(struct pool_set *set,
		struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, set_hs %p, flags %u", set, set_hs, flags);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		if (rep->remote) {
			if (util_replica_open_remote(set, r, 0)) {
				LOG(1, "cannot open remote replica no %u", r);
				return -1;
			}

			unsigned nlanes = REMOTE_NLANES;
			int ret = util_poolset_remote_open(rep, r,
					rep->repsize, 0,
					rep->part[0].addr,
					rep->part[0].size, &nlanes);
			if (ret)
				rep_hs->flags |= IS_BROKEN;

			continue;
		}

		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (access(rep->part[p].path, R_OK|W_OK) != 0) {
				LOG(1, "part file %s is not accessible",
						rep->part[p].path);
				errno = 0;
				rep_hs->part[p] |= IS_BROKEN;
				if (is_dry_run(flags))
					continue;
			}
			if (util_part_open(&rep->part[p], 0, 0)) {
				LOG(1, "opening part %s failed",
						rep->part[p].path);
				errno = 0;
				rep_hs->part[p] |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * map_all_unbroken_headers -- (internal) map all headers in a poolset,
 *                             skipping those marked as broken in a helping
 *                             structure
 */
static int
map_all_unbroken_headers(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			/* skip broken parts */
			if (replica_is_part_broken(r, p, set_hs))
				continue;

			LOG(4, "mapping header for part %u, replica %u", p, r);
			if (util_map_hdr(&rep->part[p], MAP_SHARED, 0) != 0) {
				LOG(1, "header mapping failed - part #%d", p);
				rep_hs->part[p] |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * unmap_all_headers -- (internal) unmap all headers in a poolset
 */
static int
unmap_all_headers(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		util_replica_close(set, r);

		if (rep->remote && rep->remote->rpp) {
			Rpmem_close(rep->remote->rpp);
			rep->remote->rpp = NULL;
		}
	}

	return 0;
}

/*
 * check_checksums -- (internal) check if checksums are correct for parts in
 *                    a given replica
 */
static int
check_checksums(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = REP(set_hs, r);

		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {

			/* skip broken parts */
			if (replica_is_part_broken(r, p, set_hs))
				continue;

			/* check part's checksum */
			LOG(4, "checking checksum for part %u, replica %u",
					p, r);
			struct pool_hdr *hdrp = HDR(rep, p);
			if (!util_checksum(hdrp, sizeof(*hdrp),
					&hdrp->checksum, 0)) {;
				ERR("invalid checksum of pool header");
				rep_hs->part[p] |= IS_BROKEN;
			} else if (util_is_zeroed(hdrp, sizeof(*hdrp))) {
					rep_hs->part[p] |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * check_uuids_between_parts -- (internal) check if uuids between adjacent
 *                              parts are consistent for a given replica
 */
static void
check_uuids_between_parts(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	struct replica_health_status *rep_hs = REP(set_hs, repn);

	/* check parts linkage */
	LOG(4, "checking parts linkage in replica %u", repn);
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		struct pool_hdr *hdrp = HDR(rep, p);
		struct pool_hdr *next_hdrp = HDRN(rep, p);
		int next_is_broken = replica_is_part_broken(repn, p + 1,
				set_hs);

		if (!next_is_broken) {
			int next_decoupled =
				uuidcmp(next_hdrp->prev_part_uuid,
					hdrp->uuid) ||
				uuidcmp(hdrp->next_part_uuid, next_hdrp->uuid);
			if (next_decoupled) {
				rep_hs->flags |= IS_INCONSISTENT;
				/* skip further checking */
				return;
			}
		}
	}

	/* check if all uuids for adjacent replicas are the same across parts */
	LOG(4, "checking consistency of adjacent replicas' uuids in replica %u",
			repn);
	unsigned unbroken_p = UNDEF_PART;
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (unbroken_p == UNDEF_PART) {
			unbroken_p = p;
			continue;
		}

		struct pool_hdr *hdrp = HDR(rep, p);
		int prev_differ = uuidcmp(HDR(rep, unbroken_p)->prev_repl_uuid,
				hdrp->prev_repl_uuid);
		int next_differ = uuidcmp(HDR(rep, unbroken_p)->next_repl_uuid,
				hdrp->next_repl_uuid);

		if (prev_differ || next_differ) {
			ERR("different adjacent replica UUID between parts");
			rep_hs->flags |= IS_INCONSISTENT;
			/* skip further checking */
			return;
		}
	}

	/* check poolset_uuid consistency between replica's parts */
	LOG(4, "checking consistency of poolset uuid in replica %u", repn);
	uuid_t poolset_uuid;
	int uuid_stored = 0;
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (!uuid_stored) {
			memcpy(poolset_uuid, HDR(rep, p)->poolset_uuid,
					POOL_HDR_UUID_LEN);
			uuid_stored = 1;
			continue;
		}

		if (uuidcmp(HDR(rep, p)->poolset_uuid, poolset_uuid)) {
			rep_hs->flags |= IS_INCONSISTENT;
			/* skip further checking */
			return;
		}
	}
}

/*
 * check_replicas_consistency -- (internal) check if all uuids within each
 *                               replica are consistent
 */
static int
check_replicas_consistency(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		check_uuids_between_parts(set, r, set_hs);
	}

	if (find_consistent_replica(set_hs) == UNDEF_REPLICA)
		return -1;

	return 0;
}

/*
 * check_replica_poolset_uuids - (internal) check if poolset_uuid fields are
 *                               consistent among all parts of a replica;
 *                               the replica is initially considered as
 *                               consistent
 */
static int
check_replica_poolset_uuids(struct pool_set *set, unsigned repn,
		uuid_t poolset_uuid, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, poolset_uuid %p, set_hs %p", set, repn,
			poolset_uuid, set_hs);
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (!uuidcmp(HDR(rep, p)->poolset_uuid, poolset_uuid)) {
			/*
			 * two internally consistent replicas have
			 * different poolset_uuid
			 */
			if (replica_is_replica_broken(repn, set_hs)) {
				/* mark broken replica as inconsistent */
				REP(set_hs, repn)->flags |= IS_INCONSISTENT;
			} else {
				/*
				 * two consistent unbroken replicas
				 * - cannot synchronize
				 */
				ERR("inconsistent poolset_uuid values");
				return -1;
			}
		} else {
			/*
			 * it is sufficient to check only one part
			 * from internally consistent replica
			 */
			break;
		}
	}
	return 0;
}

/*
 * check_poolset_uuids -- (internal) check if poolset_uuid fields are consistent
 *                        among all internally consistent replicas
 */
static int
check_poolset_uuids(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	unsigned r_h = replica_find_healthy_replica(set_hs);
	if (r_h == UNDEF_REPLICA) {
		ERR("no healthy replica. Cannot synchronize.");
		return -1;
	}

	uuid_t poolset_uuid;
	memcpy(poolset_uuid, HDR(REP(set, r_h), 0)->poolset_uuid,
			POOL_HDR_UUID_LEN);

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip inconsistent replicas */
		if (!replica_is_replica_consistent(r, set_hs) || r == r_h)
			continue;

		check_replica_poolset_uuids(set, r, poolset_uuid, set_hs);
	}
	return 0;
}

/*
 * check_uuids_between_replicas -- (internal) check if uuids between internally
 *                                 consistent adjacent replicas are consistent
 */
static int
check_uuids_between_replicas(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip comparing inconsistent pairs of replicas */
		if (!replica_is_replica_consistent(r, set_hs) ||
				!replica_is_replica_consistent(r + 1, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);
		struct pool_replica *rep_n = REPN(set, r);
		struct replica_health_status *rep_hs = REP(set_hs, r);
		struct replica_health_status *rep_n_hs = REPN(set_hs, r);

		/* check adjacent replica uuids for yet unbroken parts */
		unsigned p = replica_find_unbroken_part(r, set_hs);
		unsigned p_n = replica_find_unbroken_part(r + 1, set_hs);

		/* if the first part is broken, cannot compare replica uuids */
		if (p > 0) {
			rep_hs->flags |= IS_BROKEN;
			continue;
		}

		/* if the first part is broken, cannot compare replica uuids */
		if (p_n > 0) {
			rep_n_hs->flags |= IS_BROKEN;
			continue;
		}

		/* check if replica uuids are consistent between replicas */
		if (uuidcmp(HDR(rep_n, p_n)->prev_repl_uuid,
				HDR(rep, p)->uuid) || uuidcmp(
				HDR(rep, p)->next_repl_uuid,
				HDR(rep_n, p_n)->uuid)) {

			if (set->nreplicas == 1) {
				rep_hs->flags |= IS_INCONSISTENT;
			} else {
				if (replica_is_replica_broken(r, set_hs)) {
					rep_hs->flags |= IS_BROKEN;
					continue;
				}

				if (replica_is_replica_broken(r + 1, set_hs)) {
					rep_n_hs->flags |= IS_BROKEN;
					continue;
				}

				/*
				 * two unbroken and internally consistent
				 * adjacent replicas have different adjacent
				 * replica uuids - mark one as inconsistent
				 */
				rep_n_hs->flags |= IS_INCONSISTENT;
				continue;
			}
		}
	}
	return 0;
}

/*
 * check_replica_cycles -- (internal) check if healthy replicas form cycles
 *	shorter than the number of all replicas
 */
static int
check_replica_cycles(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	unsigned first_healthy;
	unsigned count_healthy = 0;
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs)) {
			count_healthy = 0;
			continue;
		}

		if (count_healthy == 0)
			first_healthy = r;

		++count_healthy;
		struct pool_hdr *hdrh =
				PART(REP(set, first_healthy), 0).hdr;
		struct pool_hdr *hdr = PART(REP(set, r), 0).hdr;
		if (uuidcmp(hdrh->uuid, hdr->next_repl_uuid) == 0 &&
				count_healthy < set->nreplicas) {
			/*
			 * healthy replicas form a cycle shorter than
			 * the number of all replicas; for the user it
			 * means that:
			 */
			ERR("there exist healthy replicas which come"
				" from a different poolset file");
			return -1;
		}
	}
	return 0;
}

/*
 * check_replica_sizes -- (internal) check if all replicas are large
 *	enough to hold data from a healthy replica
 */
static int
check_replica_sizes(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	unsigned healthy_replica = replica_find_healthy_replica(set_hs);
	if (set->poolsize < replica_get_pool_size(set, healthy_replica)) {
		ERR("some replicas are too small to hold synchronized data");
		return -1;
	}

	return 0;
}

/*
 * replica_check_poolset_health -- check if a given poolset can be considered as
 *                         healthy, and store the status in a helping structure
 */
int
replica_check_poolset_health(struct pool_set *set,
		struct poolset_health_status **set_hsp, unsigned flags)
{
	LOG(3, "set %p, set_hsp %p, flags %u", set, set_hsp, flags);
	if (replica_create_poolset_health_status(set, set_hsp)) {
		LOG(1, "creating poolset health status failed");
		return -1;
	}

	struct poolset_health_status *set_hs = *set_hsp;

	/* check if part files exist, and if not - create them, and open them */
	check_and_open_poolset_part_files(set, set_hs, flags);

	/* map all headers */
	map_all_unbroken_headers(set, set_hs);

	/* check if checksums are correct for parts in all replicas */
	check_checksums(set, set_hs);

	/* check if uuids in parts across each replica are consistent */
	if (check_replicas_consistency(set, set_hs)) {
		LOG(1, "replica consistency check failed");
		goto err;
	}

	/* check poolset_uuid values between replicas */
	if (check_poolset_uuids(set, set_hs)) {
		LOG(1, "poolset uuids check failed");
		goto err;
	}

	/* check if uuids for adjacent replicas are consistent */
	if (check_uuids_between_replicas(set, set_hs)) {
		LOG(1, "replica uuids check failed");
		goto err;
	}

	/* check if healthy replicas make up another poolset */
	if ((flags & IS_TRANSFORMED) == 0 &&
			check_replica_cycles(set, set_hs)) {
		LOG(1, "replica cycles check failed");
		goto err;
	}

	/* check if replicas are large enough */
	if (check_replica_sizes(set, set_hs)) {
		LOG(1, "replica sizes check failed");
		goto err;
	}

	if (check_store_all_sizes(set, set_hs)) {
		LOG(1, "reading pool sizes failed");
		goto err;
	}

	unmap_all_headers(set);
	util_poolset_fdclose(set);
	return 0;

err:
	unmap_all_headers(set);
	util_poolset_fdclose(set);
	replica_free_poolset_health_status(set_hs);
	return -1;
}

/*
 * replica_get_pool_size -- find the effective size (mapped) of a pool based
 *                          on metadata from given replica
 */
size_t
replica_get_pool_size(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_set_part *part = &PART(REP(set, repn), 0);
	int should_close_part = 0;
	int should_unmap_part = 0;
	if (part->fd == -1) {
		if (util_part_open(part, 0, 0))
			return set->poolsize;

		should_close_part = 1;
	}

	if (part->addr == NULL) {
		if (util_map_part(part, NULL,
				MMAP_ALIGN_UP(sizeof(PMEMobjpool)), 0,
				MAP_SHARED, 1)) {
			util_part_fdclose(part);
			return set->poolsize;
		}
		should_unmap_part = 1;
	}

	PMEMobjpool *pop = (PMEMobjpool *)part->addr;
	size_t ret = pop->heap_offset + pop->heap_size;

	if (should_unmap_part)
		util_unmap_part(part);
	if (should_close_part)
		util_part_fdclose(part);

	return ret;
}

/*
 * replica_check_part_sizes -- check if all parts are large enough
 */
int
replica_check_part_sizes(struct pool_set *set, size_t min_size)
{
	LOG(3, "set %p, min_size %zu", set, min_size);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote != NULL)
			/* skip remote replicas */
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (PART(rep, p).filesize < min_size) {
				ERR("replica %u, part %u: file is too small",
						r, p);
				return -1;
			}
		}
	}
	return 0;
}

/*
 * replica_check_local_part_dir -- check if directory for the part file
 *                                 exists
 */
int
replica_check_local_part_dir(struct pool_set *set, unsigned repn,
		unsigned partn)
{
	LOG(3, "set %p, repn %u, partn %u", set, repn, partn);
	char *path = Strdup(PART(REP(set, repn), partn).path);
	const char *dir = dirname(path);
	util_stat_t sb;
	if (util_stat(dir, &sb) != 0 || !(sb.st_mode & S_IFDIR)) {
		ERR("a directory %s for part %u in replica %u"
			" does not exist or is not accessible",
			path, partn, repn);
		Free(path);
		return -1;
	}
	Free(path);
	return 0;
}

/*
 * replica_check_part_dirs -- (internal) check if directories for part files
 *	exist
 */
int
replica_check_part_dirs(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote != NULL)
			/* skip remote replicas */
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (replica_check_local_part_dir(set, r, p))
				return -1;
		}
	}
	return 0;
}

/*
 * replica_open_replica_part_files -- open all part files for a replica
 */
int
replica_open_replica_part_files(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = set->replica[repn];
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip already opened files */
		if (rep->part[p].fd != -1)
			continue;

		if (util_part_open(&rep->part[p], 0, 0)) {
			LOG(1, "part files open failed for replica %u, part %u",
					repn, p);
			errno = EINVAL;
			goto err;
		}
	}
	return 0;

err:
	util_replica_fdclose(set->replica[repn]);
	return -1;
}

/*
 * replica_open_poolset_part_files -- open all part files for a poolset
 */
int
replica_open_poolset_part_files(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (set->replica[r]->remote)
			continue;
		if (replica_open_replica_part_files(set, r)) {
			LOG(1, "opening replica %u, part files failed", r);
			goto err;
		}
	}

	return 0;

err:
	util_poolset_fdclose(set);
	return -1;
}

/*
 * pmempool_sync -- synchronize replicas within a poolset
 */
int
pmempool_sync(const char *poolset, unsigned flags)
{
	LOG(3, "poolset %s, flags %u", poolset, flags);
	ASSERTne(poolset, NULL);

	/* check if poolset has correct signature */
	if (util_is_poolset_file(poolset) != 1) {
		ERR("file is not a poolset file");
		goto err;
	}

	/* check if flags are supported */
	if (check_flags_sync(flags)) {
		ERR("unsupported flags");
		errno = EINVAL;
		goto err;
	}

	/* open poolset file */
	int fd = util_file_open(poolset, NULL, 0, O_RDONLY);
	if (fd < 0) {
		ERR("cannot open a poolset file");
		goto err;
	}

	/* fill up pool_set structure */
	struct pool_set *set = NULL;
	if (util_poolset_parse(&set, poolset, fd)) {
		ERR("parsing input poolset failed");
		goto err_close_file;
	}
	if (set->remote) {
		if (util_remote_load()) {
			ERR("remote replication not available");
			goto err_close_file;
		}
	}

	/* sync all replicas */
	if (replica_sync(set, flags)) {
		LOG(1, "synchronization failed");
		goto err_close_all;
	}

	util_poolset_close(set, 0);
	close(fd);
	return 0;

err_close_all:
	util_poolset_close(set, 0);

err_close_file:
	close(fd);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;
}

/*
 * pmempool_transform -- alter poolset structure
 */
int
pmempool_transform(const char *poolset_src,
		const char *poolset_dst, unsigned flags)
{
	LOG(3, "poolset_src %s, poolset_dst %s, flags %u", poolset_src,
			poolset_dst, flags);
	ASSERTne(poolset_src, NULL);
	ASSERTne(poolset_dst, NULL);

	/* check if the source poolset has correct signature */
	if (util_is_poolset_file(poolset_src) != 1) {
		ERR("source file is not a poolset file");
		goto err;
	}

	/* check if the destination poolset has correct signature */
	if (util_is_poolset_file(poolset_dst) != 1) {
		ERR("destination file is not a poolset file");
		goto err;
	}

	/* check if flags are supported */
	if (check_flags_transform(flags)) {
		ERR("unsupported flags");
		errno = EINVAL;
		goto err;
	}

	/* open the source poolset file */
	int fd_in = util_file_open(poolset_src, NULL, 0, O_RDONLY);
	if (fd_in < 0) {
		ERR("cannot open source poolset file");
		goto err;
	}

	/* parse the source poolset file */
	struct pool_set *set_in = NULL;
	if (util_poolset_parse(&set_in, poolset_src, fd_in)) {
		ERR("parsing source poolset failed");
		close(fd_in);
		goto err;
	}
	close(fd_in);

	/* open the destination poolset file */
	int fd_out = util_file_open(poolset_dst, NULL, 0, O_RDONLY);
	if (fd_out < 0) {
		ERR("cannot open destination poolset file");
		goto err;
	}

	int del = 0;

	/* parse the destination poolset file */
	struct pool_set *set_out = NULL;
	if (util_poolset_parse(&set_out, poolset_dst, fd_out)) {
		ERR("parsing destination poolset failed");
		close(fd_out);
		goto err_free_poolin;
	}
	close(fd_out);

	/* check if the source poolset is of a correct type */
	if (pool_set_type(set_in) != POOL_TYPE_OBJ) {
		ERR("source poolset is of a wrong type");
		goto err_free_poolout;
	}

	/* check if the source poolset is healthy */
	struct poolset_health_status *set_in_hs = NULL;
	if (replica_check_poolset_health(set_in, &set_in_hs, flags)) {
		ERR("source poolset health check failed");
		goto err_free_poolout;
	}

	if (!replica_is_poolset_healthy(set_in_hs)) {
		ERR("source poolset is broken");
		replica_free_poolset_health_status(set_in_hs);
		goto err_free_poolout;
	}

	replica_free_poolset_health_status(set_in_hs);

	del = !is_dry_run(flags);

	/* transform poolset */
	if (replica_transform(set_in, set_out, flags)) {
		ERR("transformation failed");
		goto err_free_poolout;
	}

	util_poolset_close(set_in, 0);
	util_poolset_close(set_out, 0);
	return 0;

err_free_poolout:
	util_poolset_close(set_out, del);

err_free_poolin:
	util_poolset_close(set_in, 0);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;
}
