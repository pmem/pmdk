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
#include "palloc.h"
#include "file.h"
#include "os.h"
#include "out.h"
#include "pool_hdr.h"
#include "set.h"
#include "util.h"
#include "util_pmem.h"
#include "uuid.h"
#include "shutdown_state.h"
#include "os_dimm.h"

/*
 * check_flags_sync -- (internal) check if flags are supported for sync
 */
static int
check_flags_sync(unsigned flags)
{
	flags &= ~(unsigned)PMEMPOOL_DRY_RUN;
	flags &= ~(unsigned)PMEMPOOL_PROGRESS;
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
	flags &= ~(unsigned)PMEMPOOL_PROGRESS;
	return flags > 0;
}

/*
 * replica_get_part_data_len -- get data length for given part
 */
size_t
replica_get_part_data_len(struct pool_set *set_in, unsigned repn,
		unsigned partn)
{
	size_t alignment = set_in->replica[repn]->part[partn].alignment;
	size_t hdrsize = (set_in->options & OPTION_SINGLEHDR) ? 0 : alignment;
	return ALIGN_DOWN(set_in->replica[repn]->part[partn].filesize,
		alignment) - ((partn == 0) ? POOL_HDR_SIZE : hdrsize);
}

/*
 * replica_get_part_data_offset -- get data length before given part
 */
uint64_t
replica_get_part_data_offset(struct pool_set *set, unsigned repn,
		unsigned partn)
{
	if (partn == 0)
		return POOL_HDR_SIZE;

	return (uint64_t)set->replica[repn]->part[partn].addr -
		(uint64_t)set->replica[repn]->part[0].addr;
}

/*
 * replica_remove_part -- unlink part from replica
 */
int
replica_remove_part(struct pool_set *set, unsigned repn, unsigned partn)
{
	LOG(3, "set %p, repn %u, partn %u", set, repn, partn);
	struct pool_set_part *part = PART(REP(set, repn), partn);
	if (part->fd != -1) {
		os_close(part->fd);
		part->fd = -1;
	}

	int olderrno = errno;

	/* if the part is a device dax, clear its bad blocks */
	if (util_file_is_device_dax(part->path) &&
	    os_dimm_devdax_clear_badblocks(part->path)) {
		ERR("clearing bad blocks in device dax failed -- '%s'",
			part->path);
		errno = EIO;
		return -1;
	}

	if (util_unlink(part->path)) {
		if (errno != ENOENT) {
			ERR("!removing part %u from replica %u failed",
					partn, repn);
			return -1;
		}
	}

	errno = olderrno;
	LOG(4, "Removed part %s number %u from replica %u", part->path, partn,
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
	replica_hs->nhdrs = set->replica[repn]->nhdrs;
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
	struct replica_health_status *rhs = REP_HEALTH(set_hs, repn);
	return (rhs->flags & IS_BROKEN) ||
		(PART_HEALTH(rhs, partn) & IS_BROKEN);
}

/*
 * is_replica_broken -- check if any part in the replica is marked as broken
 */
int
replica_is_replica_broken(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);
	struct replica_health_status *r_hs = REP_HEALTH(set_hs, repn);
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
	return !(REP_HEALTH(set_hs, repn)->flags & IS_INCONSISTENT);
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
 * replica_find_unbroken_part_with_header -- find a part number in a given
 * replica, which is not marked as broken in the helping structure and contains
 * a pool header
 */
unsigned
replica_find_unbroken_part(unsigned repn, struct poolset_health_status *set_hs)
{
	LOG(3, "repn %u, set_hs %p", repn, set_hs);
	for (unsigned p = 0; p < REP_HEALTH(set_hs, repn)->nhdrs; ++p) {
		if (!replica_is_part_broken(repn, p, set_hs))
			return p;
	}
	return UNDEF_PART;
}

/*
 * poolset_count_broken_parts -- get the number of all broken parts in a poolset
 *
 * localization values:
 * ALL		- count local and remote parts,
 * LOCAL	- count local parts only,
 * REMOTE	- count remote parts (i.e. remote replicas) only.
 */
unsigned
poolset_count_broken_parts(struct pool_set *set,
		struct poolset_health_status *set_hs, int whence)
{
	unsigned n = 0;
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (set->replica[r]->remote && !(whence & REMOTE))
			continue;
		if (!set->replica[r]->remote && !(whence & LOCAL))
			continue;
		for (unsigned p = 0; p < set->replica[r]->nparts; ++p) {
			if (replica_is_part_broken(r, p, set_hs))
				++n;
		}
	}
	return n;
}

/*
 * replica_find_healthy_replica -- find a replica number which is a good source
 *                                 of data
 */
unsigned
replica_find_healthy_replica(struct poolset_health_status *set_hs)
{
	LOG(3, "set_hs %p", set_hs);

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (replica_is_replica_healthy(r, set_hs))
			return r;
	}

	return UNDEF_REPLICA;
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
		if (Rpmem_read(rep->remote->rpp, descr, POOL_HDR_SIZE,
			sizeof(pop) - POOL_HDR_SIZE, 0)) {
			return -1;
		}
	} else {
		/* round up map size to Mmap align size */
		if (util_map_part(&rep->part[0], NULL,
		    ALIGN_UP(sizeof(pop), rep->part[0].alignment),
		    0, MAP_SHARED, 1)) {
			return -1;
		}

		memcpy(&pop, rep->part[0].addr, sizeof(pop));

		util_unmap_part(&rep->part[0]);
	}

	void *dscp = (void *)((uintptr_t)&pop + sizeof(pop.hdr));

	if (!util_checksum(dscp, OBJ_DSC_P_SIZE, &pop.checksum, 0,
			0)) {
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
					rep->resvsize, &nlanes);
			if (ret)
				rep_hs->flags |= IS_BROKEN;

			continue;
		}

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = rep->part[p].path;
			if (os_access(path, R_OK|W_OK) != 0) {
				LOG(1, "part file %s is not accessible", path);
				errno = 0;
				rep_hs->part[p] |= IS_BROKEN;
				if (is_dry_run(flags))
					continue;
			}
			if (util_part_open(&rep->part[p], 0, 0)) {
				if (util_file_is_device_dax(path)) {
					LOG(1,
					"opening part on Device DAX %s failed",
					path);
					return -1;
				}
				LOG(1, "opening part %s failed", path);
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

		for (unsigned p = 0; p < rep->nhdrs; ++p) {
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
 * check_checksums_and_signatures -- (internal) check if checksums
 *                                   and signatures are correct for parts
 *                                   in a given replica
 */
static int
check_checksums_and_signatures(struct pool_set *set,
				struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		struct replica_health_status *rep_hs = REP_HEALTH(set_hs, r);

		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nhdrs; ++p) {

			/* skip broken parts */
			if (replica_is_part_broken(r, p, set_hs))
				continue;

			/* check part's checksum */
			LOG(4, "checking checksum for part %u, replica %u",
					p, r);
			struct pool_hdr *hdr;
			if (rep->remote) {
				hdr = rep->part[p].remote_hdr;
			} else {
				hdr = HDR(rep, p);
			}

			if (!util_checksum(hdr, sizeof(*hdr), &hdr->checksum, 0,
					POOL_HDR_CSUM_END_OFF)) {
				ERR("invalid checksum of pool header");
				rep_hs->part[p] |= IS_BROKEN;
			} else if (util_is_zeroed(hdr, sizeof(*hdr))) {
					rep_hs->part[p] |= IS_BROKEN;
			}

			enum pool_type type = pool_hdr_get_type(hdr);
			if (type == POOL_TYPE_UNKNOWN) {
				ERR("invalid signature");
				rep_hs->part[p] |= IS_BROKEN;
			}
		}
	}
	return 0;
}

/*
 * check_badblocks -- (internal) check if replica contains bad blocks
 */
static int
check_badblocks(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];

		/* XXX: not supported yet */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = PART(rep, p)->path;

			int exists = os_access(path, F_OK) == 0;
			if (!exists)
				continue;

			int ret = os_badblocks_check_file(path);
			if (ret < 0) {
				ERR(
					"checking replica for bad blocks failed -- '%s'",
					path);
				return -1;
			}

			if (ret > 0) {
				/* bad blocks were found */
				rep_hs->part[p] |= IS_BROKEN;
				rep_hs->flags |= IS_BROKEN;
			}
		}
	}

	return 0;
}


/*
 * check_shutdown_state -- (internal) check if replica has
 *			   healthy shutdown_state
 */
static int
check_shutdown_state(struct pool_set *set,
	struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {\
		struct pool_replica *rep = set->replica[r];
		struct replica_health_status *rep_hs = set_hs->replica[r];
		struct pool_hdr *hdrp = HDR(rep, 0);

		if (rep->remote)
			continue;

		if (hdrp == NULL) {
			/* cannot verify shutdown state */
			rep_hs->flags |= IS_BROKEN;
			continue;
		}

		struct shutdown_state curr_sds;
		shutdown_state_init(&curr_sds, NULL);
		for (unsigned p = 0; p < rep->nparts; ++p) {
			shutdown_state_add_part(&curr_sds, PART(rep, p)->path,
				NULL);
		}
		/* make a copy of sds as we shouldn't modify a pool */
		struct shutdown_state pool_sds = hdrp->sds;

		if (shutdown_state_check(&curr_sds, &pool_sds, NULL))
				rep_hs->flags |= IS_BROKEN;

	}
	return 0;
}

/*
 * check_uuids_between_parts -- (internal) check if uuids between adjacent
 *                              parts are consistent for a given replica
 */
static int
check_uuids_between_parts(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);

	/* check poolset_uuid consistency between replica's parts */
	LOG(4, "checking consistency of poolset uuid in replica %u", repn);
	uuid_t poolset_uuid;
	int uuid_stored = 0;
	unsigned part_stored = UNDEF_PART;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (!uuid_stored) {
			memcpy(poolset_uuid, HDR(rep, p)->poolset_uuid,
					POOL_HDR_UUID_LEN);
			uuid_stored = 1;
			part_stored = p;
			continue;
		}

		if (uuidcmp(HDR(rep, p)->poolset_uuid, poolset_uuid)) {
			ERR("different poolset uuids in parts from the same"
				" replica (repn %u, parts %u and %u); cannot"
				" synchronize", repn, part_stored, p);
			errno = EINVAL;
			return -1;
		}
	}

	/* check if all uuids for adjacent replicas are the same across parts */
	LOG(4, "checking consistency of adjacent replicas' uuids in replica %u",
			repn);
	unsigned unbroken_p = UNDEF_PART;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
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
			ERR("different adjacent replica UUID between parts"
				" (repn %u, parts %u and %u);"
				" cannot synchronize", repn, unbroken_p, p);
			errno = EINVAL;
			return -1;
		}
	}

	/* check parts linkage */
	LOG(4, "checking parts linkage in replica %u", repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
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
				ERR("two consecutive unbroken parts are not"
					" linked to each other (repn %u, parts"
					" %u and %u); cannot synchronize",
					repn, p, p + 1);
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
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
		if (check_uuids_between_parts(set, r, set_hs))
			return -1;
	}
	return 0;
}

/*
 * check_replica_options -- (internal) check if options are consistent in the
 *                          replica
 */
static int
check_replica_options(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	struct replica_health_status *rep_hs = REP_HEALTH(set_hs, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		struct pool_hdr *hdr = HDR(rep, p);
		if (((hdr->incompat_features & POOL_FEAT_SINGLEHDR) == 0) !=
				((set->options & OPTION_SINGLEHDR) == 0)) {
			LOG(1, "improper options are set in part %u's header in"
					" replica %u", p, repn);
			rep_hs->part[p] |= IS_BROKEN;
		}
	}
	return 0;
}

/*
 * check_options -- (internal) check if options are consistent in all replicas
 */
static int
check_options(struct pool_set *set, struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (check_replica_options(set, r, set_hs))
			return -1;
	}
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
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		if (uuidcmp(HDR(rep, p)->poolset_uuid, poolset_uuid)) {
			/*
			 * two internally consistent replicas have
			 * different poolset_uuid
			 */
			return -1;
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

		if (check_replica_poolset_uuids(set, r, poolset_uuid, set_hs)) {
			ERR("inconsistent poolset uuids between replicas %u and"
				" %u; cannot synchronize", r_h, r);
			return -1;
		}
	}
	return 0;
}

/*
 * get_replica_uuid -- (internal) get replica uuid
 */
static int
get_replica_uuid(struct pool_replica *rep, unsigned repn,
		struct poolset_health_status *set_hs, uuid_t **uuidpp)
{
	unsigned nhdrs = rep->nhdrs;
	if (!replica_is_part_broken(repn, 0, set_hs)) {
		/* the first part is not broken */
		*uuidpp = &HDR(rep, 0)->uuid;
		return 0;
	} else if (nhdrs > 1 && !replica_is_part_broken(repn, 1, set_hs)) {
		/* the second part is not broken */
		*uuidpp = &HDR(rep, 1)->prev_part_uuid;
		return 0;
	} else if (nhdrs > 1 &&
			!replica_is_part_broken(repn, nhdrs - 1, set_hs)) {
		/* the last part is not broken */
		*uuidpp = &HDR(rep, nhdrs - 1)->next_part_uuid;
		return 0;
	} else {
		/* cannot get replica uuid */
		return -1;
	}
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

		/* get uuids of the two adjacent replicas */
		uuid_t *rep_uuidp = NULL;
		uuid_t *rep_n_uuidp = NULL;
		unsigned r_n = REPN_HEALTHidx(set_hs, r);
		if (get_replica_uuid(rep, r, set_hs, &rep_uuidp))
			LOG(2, "cannot get replica uuid, replica %u", r);
		if (get_replica_uuid(rep_n, r_n, set_hs, &rep_n_uuidp))
			LOG(2, "cannot get replica uuid, replica %u", r_n);

		/*
		 * check if replica uuids are consistent between two adjacent
		 * replicas
		 */
		unsigned p = replica_find_unbroken_part(r, set_hs);
		unsigned p_n = replica_find_unbroken_part(r_n, set_hs);
		if (p_n != UNDEF_PART && rep_uuidp != NULL &&
				uuidcmp(*rep_uuidp,
					HDR(rep_n, p_n)->prev_repl_uuid)) {
			ERR(
			"inconsistent replica uuids between replicas %u and %u",
				r, r_n);
			return -1;
		}
		if (p != UNDEF_PART && rep_n_uuidp != NULL &&
				uuidcmp(*rep_n_uuidp,
					HDR(rep, p)->next_repl_uuid)) {
			ERR(
			"inconsistent replica uuids between replicas %u and %u",
				r, r_n);
			return -1;
		}

		/*
		 * check if replica uuids on borders of a broken replica are
		 * consistent
		 */
		unsigned r_nn = REPN_HEALTHidx(set_hs, r_n);
		if (set->nreplicas > 1 && p != UNDEF_PART &&
				replica_is_replica_broken(r_n, set_hs) &&
				replica_is_replica_consistent(r_nn, set_hs)) {
			unsigned p_nn =
				replica_find_unbroken_part(r_nn, set_hs);
			if (p_nn == UNDEF_PART) {
				LOG(2,
				"cannot compare uuids on borders of replica %u",
					r);
				continue;
			}
			struct pool_replica *rep_nn = REP(set, r_nn);
			if (uuidcmp(HDR(rep, p)->next_repl_uuid,
					HDR(rep_nn, p_nn)->prev_repl_uuid)) {
				ERR(
				"inconsistent replica uuids on borders of replica %u",
					r);
				return -1;
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
			PART(REP(set, first_healthy), 0)->hdr;
		struct pool_hdr *hdr = PART(REP(set, r), 0)->hdr;
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
	ssize_t pool_size = -1;
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip broken replicas */
		if (!replica_is_replica_healthy(r, set_hs))
			continue;

		/* get the size of a pool in the replica */
		ssize_t replica_pool_size;
		if (REP(set, r)->remote)
			/* XXX: no way to get the size of a remote pool yet */
			replica_pool_size = (ssize_t)set->poolsize;
		else
			replica_pool_size = replica_get_pool_size(set, r);

		if (replica_pool_size < 0) {
			LOG(1, "getting pool size from replica %u failed", r);
			set_hs->replica[r]->flags |= IS_BROKEN;
			continue;
		}

		/* check if the pool is bigger than minimum size */
		enum pool_type type = pool_hdr_get_type(HDR(REP(set, r), 0));
		if ((size_t)replica_pool_size < pool_get_min_size(type)) {
			LOG(1,
			"pool size from replica %u is smaller than the minimum size allowed for the pool",
			r);
			set_hs->replica[r]->flags |= IS_BROKEN;
			continue;
		}

		/* check if each replica is big enough to hold the pool data */
		if (set->poolsize < (size_t)replica_pool_size) {
			ERR(
			"some replicas are too small to hold synchronized data");
			return -1;
		}

		if (pool_size < 0) {
			pool_size = replica_pool_size;
			continue;
		}

		/* check if pools in all healthy replicas are of equal size */
		if (pool_size != replica_pool_size) {
			ERR("pool sizes from different replicas differ");
			return -1;
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
		struct poolset_health_status **set_hsp, unsigned flags,
		PMEM_progress_cb progress_cb)
{
	LOG(3, "set %p, set_hsp %p, flags %u", set, set_hsp, flags);

	char *msg = "Checking poolset health";
	size_t progress_max = 11;
	if (progress_cb)
		progress_cb(msg, 0, progress_max);

	if (replica_create_poolset_health_status(set, set_hsp)) {
		LOG(1, "creating poolset health status failed");
		util_break_progress(progress_cb);
		return -1;
	}
	if (progress_cb)
		progress_cb(msg, 1, progress_max);


	struct poolset_health_status *set_hs = *set_hsp;

	/* check if part files exist and are accessible */
	if (check_and_open_poolset_part_files(set, set_hs, flags)) {
		LOG(1, "poolset part files check failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 2, progress_max);

	/* map all headers */
	map_all_unbroken_headers(set, set_hs);
	if (progress_cb)
		progress_cb(msg, 3, progress_max);

	/*
	 * Check if checksums and signatures are correct for all parts
	 * in all replicas.
	 */
	check_checksums_and_signatures(set, set_hs);
	if (progress_cb)
		progress_cb(msg, 4, progress_max);

	/* check if option flags are consistent */
	if (check_options(set, set_hs)) {
		LOG(1, "flags check failed");
		goto err;
	}

	if (check_badblocks(set, set_hs)) {
		LOG(1, "replica bad_blocks check failed");
		goto err;
	}

	if (check_shutdown_state(set, set_hs)) {
		LOG(1, "replica shutdown_state check failed");
		goto err;
	}

	/* check if uuids in parts across each replica are consistent */
	if (check_replicas_consistency(set, set_hs)) {
		LOG(1, "replica consistency check failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 5, progress_max);


	/* check poolset_uuid values between replicas */
	if (check_poolset_uuids(set, set_hs)) {
		LOG(1, "poolset uuids check failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 6, progress_max);

	/* check if uuids for adjacent replicas are consistent */
	if (check_uuids_between_replicas(set, set_hs)) {
		LOG(1, "replica uuids check failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 7, progress_max);

	/* check if healthy replicas make up another poolset */
	if (check_replica_cycles(set, set_hs)) {
		LOG(1, "replica cycles check failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 8, progress_max);

	/* check if replicas are large enough */
	if (check_replica_sizes(set, set_hs)) {
		LOG(1, "replica sizes check failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 9, progress_max);


	if (check_store_all_sizes(set, set_hs)) {
		LOG(1, "reading pool sizes failed");
		goto err;
	}
	if (progress_cb)
		progress_cb(msg, 10, progress_max);

	unmap_all_headers(set);
	util_poolset_fdclose_always(set);
	if (progress_cb)
		progress_cb(msg, 11, progress_max);

	return 0;

err:
	errno = EINVAL;
	unmap_all_headers(set);
	util_poolset_fdclose_always(set);
	replica_free_poolset_health_status(set_hs);
	util_break_progress(progress_cb);
	return -1;
}

/*
 * replica_get_pool_size -- find the effective size (mapped) of a pool based
 *                          on metadata from given replica
 */
ssize_t
replica_get_pool_size(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_set_part *part = PART(REP(set, repn), 0);
	int should_close_part = 0;
	int should_unmap_part = 0;
	if (part->fd == -1) {
		if (util_part_open(part, 0, 0))
			return -1;

		should_close_part = 1;
	}

	if (part->addr == NULL) {
		if (util_map_part(part, NULL,
		    ALIGN_UP(sizeof(PMEMobjpool), part->alignment), 0,
		    MAP_SHARED, 1)) {
			util_part_fdclose(part);
			return -1;
		}
		should_unmap_part = 1;
	}

	PMEMobjpool *pop = (PMEMobjpool *)part->addr;
	ssize_t ret = (ssize_t)(pop->heap_offset + pop->heap_size);

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
			if (PART(rep, p)->filesize < min_size) {
				ERR("replica %u, part %u: file is too small",
						r, p);
				errno = EINVAL;
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
	char *path = Strdup(PART(REP(set, repn), partn)->path);
	const char *dir = dirname(path);
	os_stat_t sb;
	if (os_stat(dir, &sb) != 0 || !(sb.st_mode & S_IFDIR)) {
		ERR("directory %s for part %u in replica %u"
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
	util_poolset_fdclose_always(set);
	return -1;
}

/*
 * util_memcpy_persist -- copy and persist data and report progress of the
 *                        operation
 */
void
util_memcpy_persist(int is_pmem, void *to, const void *from, size_t size,
		const char *msg, PMEM_progress_cb progress_cb)
{
	LOG(3, "to %p, from %p, size %zu, msg %s, progress_cb %p", to, from,
			size, msg, progress_cb);

	if (progress_cb == NULL) {
		memcpy(to, from, size);
		util_persist(is_pmem, to, size);
	} else {
		if (msg == NULL || *msg == '\0')
			msg = "Copying data";

		size_t off = 0;
		size_t next_off = 0;
		progress_cb(msg, 0, size);
		for (unsigned i = 0; i < 100; ++i) {
			next_off = (size * (i + 1) + 99) / 100;
			memcpy(ADDR_SUM(to, off), ADDR_SUM(from, off),
					next_off - off);
			util_persist(is_pmem, ADDR_SUM(to, off),
					next_off - off);
			progress_cb(msg, next_off, size);
			off = next_off;
		}
	}
}

int
util_rpmem_read(RPMEMpool *rpp, void *buff, size_t offset, size_t length,
		unsigned lane, const char *msg, PMEM_progress_cb progress_cb)
{
	if (progress_cb == NULL) {
		return Rpmem_read(rpp, buff, offset, length, lane);
	} else {
		if (msg == NULL || *msg == '\0')
			msg = "Reading remote data";

		size_t off = 0;
		size_t next_off = 0;
		int ret = 0;
		progress_cb(msg, 0, length);
		for (unsigned i = 0; i < 100; ++i) {
			next_off = (length * (i + 1) + 99) / 100;
			ret = Rpmem_read(rpp, ADDR_SUM(buff, off),
					offset + off, next_off - off, lane);
			if (unlikely(ret)) {
				progress_cb(NULL, 0, 0);
				break;
			} else {
				progress_cb(msg, next_off, length);
			}
			off = next_off;
		}
		return ret;
	}
}

int
util_rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length,
		unsigned lane, const char *msg, PMEM_progress_cb progress_cb)
{
	if (progress_cb == NULL) {
		return Rpmem_persist(rpp, offset, length, lane);
	} else {
		if (msg == NULL || *msg == '\0')
			msg = "Persisting data";

		size_t off = 0;
		size_t next_off = 0;
		int ret = 0;
		progress_cb(msg, 0, length);
		for (unsigned i = 0; i < 100; ++i) {
			next_off = (length * (i + 1) + 99) / 100;
			ret = Rpmem_persist(rpp, offset + off,
					next_off - off, lane);
			if (unlikely(ret)) {
				progress_cb(NULL, 0, 0);
				break;
			} else {
				progress_cb(msg, next_off, length);
			}
			off = next_off;
		}
		return ret;
	}
}

/*
 * util_break_progress -- break reporting progress of the current operation
 */
void
util_break_progress(PMEM_progress_cb progress_cb)
{
	if (progress_cb)
		progress_cb(NULL, 0, 0);
}

/*
 * pmempool_syncU -- synchronize replicas within a poolset
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_syncU(const char *poolset, unsigned flags, ...)
{
	PMEM_progress_cb progress_cb = NULL;
	if (flags & PMEMPOOL_PROGRESS) {
		va_list arg;
		va_start(arg, flags);
		progress_cb = va_arg(arg, PMEM_progress_cb);
		va_end(arg);
	}

	LOG(3, "poolset %s, flags %u, progress_cb %p", poolset, flags,
			progress_cb);
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

	if (set->nreplicas == 1) {
		ERR("no replica(s) found in the pool set");
		errno = EINVAL;
		goto err_close_file;
	}

	if (set->remote && util_remote_load()) {
		ERR("remote replication not available");
		errno = ENOTSUP;
		goto err_close_file;
	}

	/* sync all replicas */
	if (replica_sync(set, NULL, flags, progress_cb)) {
		LOG(1, "synchronization failed");
		goto err_close_all;
	}

	util_poolset_close(set, DO_NOT_DELETE_PARTS);
	os_close(fd);
	return 0;

err_close_all:
	util_poolset_close(set, DO_NOT_DELETE_PARTS);

err_close_file:
	os_close(fd);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;

}

#ifndef _WIN32
/*
 * pmempool_sync -- synchronize replicas within a poolset
 *
 * if flag PMEMPOOL_PROGRESS is set and an additional argument is a function of
 * PMEM_progress_cb type, the progress of the operation will be reported using
 * this function
 */
int
pmempool_sync(const char *poolset, unsigned flags, ...)
{
	if (flags & PMEMPOOL_PROGRESS) {
		va_list arg;
		va_start(arg, flags);
		PMEM_progress_cb progress_cb = va_arg(arg, PMEM_progress_cb);
		va_end(arg);
		return pmempool_syncU(poolset, flags, progress_cb);
	} else {
		return pmempool_syncU(poolset, flags);
	}
}
#else
/*
 * pmempool_syncW -- synchronize replicas within a poolset in widechar
 *
 * if flag PMEMPOOL_PROGRESS is set and an additional argument is a function of
 * PMEM_progress_cb type, the progress of the operation will be reported using
 * this function
 */
int
pmempool_syncW(const wchar_t *poolset, unsigned flags, ...)
{
	char *path = util_toUTF8(poolset);
	if (path == NULL) {
		ERR("Invalid poolest file path.");
		return -1;
	}

	int ret;
	if (flags & PMEMPOOL_PROGRESS) {
		va_list arg;
		va_start(arg, flags);
		PMEM_progress_cb progress_cb = va_arg(arg, PMEM_progress_cb);
		va_end(arg);
		ret = pmempool_syncU(path, flags, progress_cb);
	} else {
		ret = pmempool_syncU(path, flags);
	}

	util_free_UTF8(path);
	return ret;
}
#endif

/*
 * pmempool_transformU -- alter poolset structure
 */
#ifndef _WIN32
static inline
#endif
int
pmempool_transformU(const char *poolset_src, const char *poolset_dst,
		unsigned flags, ...)
{
	PMEM_progress_cb progress_cb = NULL;
	if (flags & PMEMPOOL_PROGRESS) {
		va_list arg;
		va_start(arg, flags);
		progress_cb = va_arg(arg, PMEM_progress_cb);
		va_end(arg);
	}

	LOG(3, "poolset_src %s, poolset_dst %s, flags %u, progress_cb %p",
			poolset_src, poolset_dst, flags, progress_cb);

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
		os_close(fd_in);
		goto err;
	}
	os_close(fd_in);

	/* open the destination poolset file */
	int fd_out = util_file_open(poolset_dst, NULL, 0, O_RDONLY);
	if (fd_out < 0) {
		ERR("cannot open destination poolset file");
		goto err;
	}

	enum del_parts_mode del = DO_NOT_DELETE_PARTS;

	/* parse the destination poolset file */
	struct pool_set *set_out = NULL;
	if (util_poolset_parse(&set_out, poolset_dst, fd_out)) {
		ERR("parsing destination poolset failed");
		os_close(fd_out);
		goto err_free_poolin;
	}
	os_close(fd_out);

	/* check if the source poolset is of a correct type */
	enum pool_type ptype = pool_set_type(set_in);
	if (ptype != POOL_TYPE_OBJ) {
		ERR("transform is not supported for given pool type: %s",
				pool_get_pool_type_str(ptype));
		goto err_free_poolout;
	}

	/* load remote library if needed */
	if (set_in->remote && util_remote_load()) {
		ERR("remote replication not available");
		goto err_free_poolout;
	}
	if (set_out->remote && util_remote_load()) {
		ERR("remote replication not available");
		goto err_free_poolout;
	}

	del = is_dry_run(flags) ? DO_NOT_DELETE_PARTS : DELETE_CREATED_PARTS;

	/* transform poolset */
	if (replica_transform(set_in, set_out, flags, progress_cb)) {
		LOG(1, "transformation failed");
		goto err_free_poolout;
	}

	util_poolset_close(set_in, DO_NOT_DELETE_PARTS);
	util_poolset_close(set_out, DO_NOT_DELETE_PARTS);
	return 0;

err_free_poolout:
	util_poolset_close(set_out, del);

err_free_poolin:
	util_poolset_close(set_in, DO_NOT_DELETE_PARTS);

err:
	if (errno == 0)
		errno = EINVAL;

	return -1;
}

#ifndef _WIN32
/*
 * pmempool_transform -- alter poolset structure
 *
 * if flag PMEMPOOL_PROGRESS is set and an additional argument is a function of
 * PMEM_progress_cb type, the progress of the operation will be reported using
 * this function
 */
int
pmempool_transform(const char *poolset_src,
	const char *poolset_dst, unsigned flags, ...)
{
	if (flags & PMEMPOOL_PROGRESS) {
		va_list arg;
		va_start(arg, flags);
		PMEM_progress_cb progress_cb = va_arg(arg, PMEM_progress_cb);
		va_end(arg);
		return pmempool_transformU(poolset_src, poolset_dst, flags,
				progress_cb);
	} else {
		return pmempool_transformU(poolset_src, poolset_dst, flags);
	}
}
#else
/*
 * pmempool_transformW -- alter poolset structure in widechar
 *
 * if flag PMEMPOOL_PROGRESS is set and an additional argument is a function of
 * PMEM_progress_cb type, the progress of the operation will be reported using
 * this function
 */
int
pmempool_transformW(const wchar_t *poolset_src,
	const wchar_t *poolset_dst, unsigned flags, ...)
{
	char *path_src = util_toUTF8(poolset_src);
	if (path_src == NULL) {
		ERR("Invalid source poolest file path.");
		return -1;
	}

	char *path_dst = util_toUTF8(poolset_dst);
	if (path_dst == NULL) {
		ERR("Invalid destination poolest file path.");
		Free(path_src);
		return -1;
	}

	int ret;
	if (flags & PMEMPOOL_PROGRESS) {
		va_list arg;
		va_start(arg, flags);
		PMEM_progress_cb progress_cb = va_arg(arg, PMEM_progress_cb);
		va_end(arg);
		ret = pmempool_transformU(path_src, path_dst, flags,
				progress_cb);
	} else {
		ret = pmempool_transformU(path_src, path_dst, flags);
	}

	util_free_UTF8(path_src);
	util_free_UTF8(path_dst);
	return ret;
}
#endif
