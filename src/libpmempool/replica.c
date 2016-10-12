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

#include "obj.h"
#include "file.h"
#include "out.h"
#include "pool_hdr.h"
#include "set.h"
#include "util.h"
#include "uuid.h"

/*
 * replica_get_part_data_len -- get data length for given part
 */
size_t
replica_get_part_data_len(struct pool_set *set_in, unsigned repn,
		unsigned partn)
{
	return PAGE_ALIGNED_DOWN_SIZE(
			set_in->replica[repn]->part[partn].filesize) -
			POOL_HDR_SIZE;
}

/*
 * replica_get_part_range_data_len -- get data length in given range
 */
size_t
replica_get_part_range_data_len(struct pool_set *set, unsigned repn,
		unsigned pstart, unsigned pend)
{
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
	struct pool_set_part *part = &PART(REP(set, repn), partn);
	if (part->fd != -1)
		close(part->fd);

	if (unlink(part->path)) {
		if (errno != ENOENT) {
			ERR("Removing part %u from replica %u failed",
					partn, repn);
			return -1;
		}
	}
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
	struct poolset_health_status *set_hs, unsigned r)
{
	struct pool_replica *rep = set->replica[r];
	struct pmemobjpool pop;

	if (rep->remote) {
		memcpy(&pop.hdr, rep->part[0].hdr, sizeof(pop.hdr));
		void *descr = (void *)((uintptr_t)&pop + POOL_HDR_SIZE);
		if (Rpmem_read(rep->remote->rpp, descr, 0,
				sizeof(pop) - POOL_HDR_SIZE)) {
			return -1;
		}
	} else {
		if (util_map_part(&rep->part[0], NULL, sizeof(pop),
				0, MAP_PRIVATE|MAP_NORESERVE)) {
			return -1;
		}

		memcpy(&pop, rep->part[0].addr, sizeof(pop));

		util_unmap_part(&rep->part[0]);
	}

	void *dscp = (void *)((uintptr_t)&pop + sizeof(pop.hdr));

	if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop.checksum, 0)) {
		set_hs->replica[r]->flags |= IS_BROKEN;
		return 0;
	}

	set_hs->replica[r]->pool_size = pop.heap_offset + pop.heap_size;

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
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		if (rep->remote) {
			if (util_replica_open_remote(set, r, 0)) {
				LOG(1, "Cannot open remote replica");
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
				LOG(1, "Part file %s is not accessible",
						rep->part[p].path);
				errno = 0;
				rep_hs->part[p] |= IS_BROKEN;
				if (is_dry_run(flags))
					continue;
			}
			if (util_part_open(&rep->part[p], 0, 0)) {
				LOG(1, "Opening part %s failed",
						rep->part[p].path);
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
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			/* skip broken parts */
			if (replica_is_part_broken(r, p, set_hs))
				continue;

			if (util_map_hdr(&rep->part[p], MAP_SHARED) != 0) {
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
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		util_replica_close(set, r);

		if (rep->remote && rep->remote->rpp)
			Rpmem_close(rep->remote->rpp);
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
			struct pool_hdr *hdrp = HDR(rep, p);
			if (!util_checksum(hdrp, sizeof(*hdrp),
					&hdrp->checksum, 0)) {;
				ERR("invalid checksum of pool header");
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
	struct pool_replica *rep = REP(set, repn);
	struct replica_health_status *rep_hs = REP(set_hs, repn);

	/* check parts linkage */
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		struct pool_hdr *hdrp = HDR(rep, p);
		struct pool_hdr *next_hdrp = HDR(rep, p + 1);
		int next_is_broken = replica_is_part_broken(repn, p + 1,
				set_hs);

		if (!next_is_broken) {
			int next_decoupled =
				memcmp(next_hdrp->prev_part_uuid,
					hdrp->uuid, POOL_HDR_UUID_LEN) ||
				memcmp(hdrp->next_part_uuid, next_hdrp->uuid,
					POOL_HDR_UUID_LEN);
			if (next_decoupled) {
				rep_hs->flags |= IS_INCONSISTENT;
				/* skip further checking */
				return;
			}
		}
	}

	/* check if all uuids for adjacent replicas are the same across parts */
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
		int prev_differ = memcmp(HDR(rep, unbroken_p)->prev_repl_uuid,
				hdrp->prev_repl_uuid, POOL_HDR_UUID_LEN);
		int next_differ = memcmp(HDR(rep, unbroken_p)->next_repl_uuid,
				hdrp->next_repl_uuid, POOL_HDR_UUID_LEN);

		if (prev_differ || next_differ) {
			ERR("different adjacent replica UUID between parts");
			rep_hs->flags |= IS_INCONSISTENT;
			/* skip further checking */
			return;
		}
	}

	/* check poolset_uuid consistency between replica's parts */
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

		if (memcmp(HDR(rep, p)->poolset_uuid, poolset_uuid,
				POOL_HDR_UUID_LEN)) {
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
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (!memcmp(HDR(rep, p)->poolset_uuid, poolset_uuid,
				POOL_HDR_UUID_LEN)) {
			/*
			 * two internally consistent replicas have
			 * different poolset_uuid
			 */
			if (replica_is_replica_broken(repn, set_hs)) {
				/* mark broken replica as inconsistent */
				REP(set_hs, repn)->flags |=
						IS_INCONSISTENT;
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
	unsigned r_h = replica_find_healthy_replica(set_hs);
	if (r_h == UNDEF_REPLICA) {
		ERR("No healthy replica. Cannot synchronize.");
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
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip comparing inconsistent pairs of replicas */
		if (!replica_is_replica_consistent(r, set_hs) ||
				!replica_is_replica_consistent(r + 1, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);
		struct pool_replica *rep_n = REP(set, r + 1);
		struct replica_health_status *rep_hs = REP(set_hs, r);
		struct replica_health_status *rep_n_hs = REP(set_hs, r + 1);

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
		if (memcmp(HDR(rep_n, p_n)->prev_repl_uuid,
				HDR(rep, p)->uuid, POOL_HDR_UUID_LEN) ||
				memcmp(HDR(rep, p)->next_repl_uuid,
				HDR(rep_n, p_n)->uuid, POOL_HDR_UUID_LEN)) {

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

				// two unbroken and internally consistent
				// adjacent replicas have different adjacent
				// replica uuids - mark one as inconsistent
				rep_n_hs->flags |= IS_INCONSISTENT;
				continue;
			}
		}
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
	if (replica_create_poolset_health_status(set, set_hsp)) {
		LOG(1, "Creating poolset health status failed");
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
		LOG(1, "Replica consistency check failed");
		goto err;
	}

	/* check poolset_uuid values between replicas */
	if (check_poolset_uuids(set, set_hs)) {
		LOG(1, "Poolset uuids check failed");
		goto err;
	}

	/* check if uuids for adjacent replicas are consistent */
	if (check_uuids_between_replicas(set, set_hs)) {
		LOG(1, "Replica uuids check failed");
		goto err;
	}

	if (check_store_all_sizes(set, set_hs)) {
		LOG(1, "Reading pool sizes failed");
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
	struct pool_set_part *part = &PART(REP(set, repn), 0);
	int should_close_part = 0;
	if (part->fd == -1) {
		if (util_part_open(part, 0, 0))
			return set->poolsize;

		if (util_map_part(part, NULL, sizeof(PMEMobjpool), 0,
				MAP_PRIVATE|MAP_NORESERVE)) {
			util_part_fdclose(part);
			return set->poolsize;
		}
		should_close_part = 1;
	}

	PMEMobjpool *pop = (PMEMobjpool *)part->addr;
	size_t ret = pop->heap_offset + pop->heap_size;

	if (should_close_part) {
		util_unmap_part(part);
		util_part_fdclose(part);
	}
	return ret;
}

/*
 * replica_open_replica_part_files -- open all part files for a replica
 */
int
replica_open_replica_part_files(struct pool_set *set, unsigned repn)
{
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
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (set->replica[r]->remote)
			continue;
		if (replica_open_replica_part_files(set, r)) {
			LOG(1, "Opening replica %u, part files failed", r);
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
	ASSERTne(poolset, NULL);

	/* check if poolset has correct signature */
	if (util_is_poolset_file(poolset) != 1) {
		ERR("File is not a poolset file");
		goto err;
	}

	/* open poolset file */
	int fd = util_file_open(poolset, NULL, 0, O_RDONLY);
	if (fd < 0) {
		ERR("Cannot open a poolset file");
		goto err;
	}

	/* fill up pool_set structure */
	struct pool_set *set = NULL;
	if (util_poolset_parse(&set, poolset, fd)) {
		ERR("Parsing input poolset failed");
		goto err_close_file;
	}
	if (set->remote) {
		if (util_remote_load()) {
			ERR("remote replication not available");
			goto err_close_file;
		}
	}

	/* sync all replicas */
	if (sync_replica(set, flags)) {
		ERR("Synchronization failed");
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
pmempool_transform(const char *poolset_file_src,
		const char *poolset_file_dst, unsigned flags)
{
	ASSERTne(poolset_file_src, NULL);
	ASSERTne(poolset_file_dst, NULL);

	/* check if the source poolset has correct signature */
	if (util_is_poolset_file(poolset_file_src) != 1) {
		ERR("Source file is not a poolset file");
		goto err;
	}

	/* check if the destination poolset has correct signature */
	if (util_is_poolset_file(poolset_file_dst) != 1) {
		ERR("Destination file is not a poolset file");
		goto err;
	}

	/* open the source poolset file */
	int fd_in = util_file_open(poolset_file_src, NULL, 0, O_RDONLY);
	if (fd_in < 0) {
		ERR("Cannot open source poolset file");
		goto err;
	}

	/* parse the source poolset file */
	struct pool_set *set_in = NULL;
	if (util_poolset_parse(&set_in, poolset_file_src, fd_in)) {
		ERR("Parsing source poolset failed");
		close(fd_in);
		goto err;
	}
	close(fd_in);

	/* open the destination poolset file */
	int fd_out = util_file_open(poolset_file_dst, NULL, 0, O_RDONLY);
	if (fd_out < 0) {
		ERR("Cannot open destination poolset file");
		goto err;
	}

	/* parse the destination poolset file */
	struct pool_set *set_out = NULL;
	if (util_poolset_parse(&set_out, poolset_file_dst, fd_out)) {
		ERR("Parsing destination poolset failed");
		close(fd_out);
		goto err_free_poolin;
	}
	close(fd_out);

	/* check if the source poolset is of a correct type */
	if (pool_set_type(set_in) != POOL_TYPE_OBJ) {
		ERR("Source poolset is of a wrong type");
		goto err_free_poolout;
	}

	/* check if the source poolset is healthy */
	struct poolset_health_status *set_in_hs = NULL;
	if (replica_check_poolset_health(set_in, &set_in_hs, flags)) {
		ERR("Source poolset health check failed");
		goto err_free_poolout;
	}
	if (!replica_is_poolset_healthy(set_in_hs)) {
		ERR("Source poolset is broken");
		replica_free_poolset_health_status(set_in_hs);
		goto err_free_poolout;
	}
	replica_free_poolset_health_status(set_in_hs);

	/* transform poolset */
	if (transform_replica(set_in, set_out, flags)) {
		ERR("Transformation failed");
		goto err_free_poolin;
	}

	util_poolset_close(set_in, 0);
	util_poolset_close(set_out, 0);
	return 0;

err_free_poolout:
	util_poolset_close(set_out, 0);

err_free_poolin:
	util_poolset_close(set_in, 0);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;
}
