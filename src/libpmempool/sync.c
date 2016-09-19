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
 * sync.c -- a module for poolset synchronizing
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>

#include "libpmem.h"
#include "replica.h"
#include "out.h"

#define ADDR_SUM(vp, lp) ((void *)((char *)(vp) + lp))

/*
 * recreate_broken_parts -- (internal) create parts in place of the broken ones
 */
static int
recreate_broken_parts(struct pool_set *set,
		struct poolset_health_status *set_hs, unsigned flags)
{
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		struct pool_replica *broken_r = set->replica[r];

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; ++p) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs))
				continue;

			/* remove parts from broken replica */
			if (!is_dry_run(flags)) {
				if (replica_remove_part(set, r, p)) {
					ERR("Cannot remove part");
					errno = EINVAL;
					return -1;
				}
			}

			/* create removed part and open it */
			if (util_part_open(&broken_r->part[p], 0,
					!is_dry_run(flags))) {
				ERR("Cannot open/create parts");
				errno = EINVAL;
				return -1;
			}

			/* generate new uuids for removed parts */
			if (util_uuid_generate(broken_r->part[p].uuid) < 0) {
				ERR("Cannot generate pool set part UUID");
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * fill_struct_part_uuids -- (internal) set part uuids in pool_set structure
 */
static void
fill_struct_part_uuids(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	struct pool_replica *replica = REP(set, repn);
	struct pool_hdr *hdrp;
	for (unsigned p = 0; p < replica->nparts; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		hdrp = HDR(replica, p);
		memcpy(replica->part[p].uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
	}
}

/*
 * fill_struct_uuids -- (internal) fill fields in pool_set needed for further
 *                      altering of uuids
 */
static void
fill_struct_uuids(struct pool_set *set, unsigned src_replica,
		struct poolset_health_status *set_hs)
{
	/* set poolset uuid */
	struct pool_hdr *src_hdr0 = HDR(REP(set, src_replica), 0);
	memcpy(set->uuid, src_hdr0->poolset_uuid, POOL_HDR_UUID_LEN);

	/* set unbroken parts' uuids */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		fill_struct_part_uuids(set, r, set_hs);
	}
}

/*
 * create_headers_for_broken_parts -- (internal) create headers for all new
 *                                    parts created in place of the broken ones
 */
static int
create_headers_for_broken_parts(struct pool_set *set, unsigned src_replica,
		struct poolset_health_status *set_hs)
{
	struct pool_hdr *src_hdr = HDR(REP(set, src_replica), 0);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken replicas */
		if (!replica_is_replica_broken(r, set_hs))
			continue;

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; p++) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs))
				continue;

			if (util_header_create(set, r, p,
					src_hdr->signature, src_hdr->major,
					src_hdr->compat_features,
					src_hdr->incompat_features,
					src_hdr->ro_compat_features,
					NULL, NULL, NULL) != 0) {
				LOG(2, "part headers create failed for"
						" replica %u part %u", r, p);
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * copy_data_to_broken_parts -- (internal) copy data to all parts created
 *                              in place of the broken ones
 */
static int
copy_data_to_broken_parts(struct pool_set *set, unsigned healthy_replica,
		unsigned flags, struct poolset_health_status *set_hs)
{
	size_t poolsize = replica_get_pool_size(set);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken and consistent replicas */
		if (!replica_is_replica_broken(r, set_hs) &&
				!replica_is_replica_inconsistent(r, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);
		struct pool_replica *rep_h = REP(set, healthy_replica);

		for (unsigned p = 0; p < rep->nparts; ++p) {
			/* skip unbroken parts from consistent replicas */
			if (!replica_is_part_broken(r, p, set_hs) &&
				!replica_is_replica_inconsistent(r, set_hs))
				continue;

			const struct pool_set_part *part = &rep->part[p];

			size_t off = replica_get_part_data_offset(set, r, p);
			size_t len = replica_get_part_data_len(set, r, p);

			/* do not allow copying too much data */
			if (off >= poolsize)
				continue;

			if (off + len > poolsize)
				len = poolsize - off;

			void *src_addr = ADDR_SUM(rep_h->part[0].addr, off);

			/* First part of replica is mapped with header */
			size_t fpoff = (p == 0) ? POOL_HDR_SIZE : 0;

			/* copy all data */
			if (!is_dry_run(flags))
				memcpy(ADDR_SUM(part->addr, fpoff),
						src_addr, len);
		}
	}
	return 0;
}

/*
 * grant_broken_parts_perm -- (internal) set RW permission rights to all
 *                            the parts created in place of the broken ones
 */
static int
grant_broken_parts_perm(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken replicas */
		if (!replica_is_replica_broken(r, set_hs))
			continue;

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; p++) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs))
				continue;

			/* XXX set rights to those of existing part files */
			if (chmod(PART(REP(set, r), p).path,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP)) {
				ERR("Cannot set permission rights for created"
					" parts: replica %u, part %u", r, p);
				errno = EPERM;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * update_parts_linkage -- (internal) set uuids linking recreated parts within
 *                         a replica
 */
static int
update_parts_linkage(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nparts; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		struct pool_hdr *prev_hdrp = HDR(rep, p - 1);
		struct pool_hdr *next_hdrp = HDR(rep, p + 1);

		/* set uuids in the current part */
		memcpy(hdrp->prev_part_uuid, PART(rep, p - 1).uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_part_uuid, PART(rep, p + 1).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1);

		/* set uuids in the previous part */
		memcpy(prev_hdrp->next_part_uuid, PART(rep, p).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(prev_hdrp, sizeof(*prev_hdrp),
				&prev_hdrp->checksum, 1);

		/* set uuids in the next part */
		memcpy(next_hdrp->prev_part_uuid, PART(rep, p).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(next_hdrp, sizeof(*next_hdrp),
				&next_hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(hdrp, sizeof(*hdrp));
		pmem_msync(prev_hdrp, sizeof(*prev_hdrp));
		pmem_msync(next_hdrp, sizeof(*next_hdrp));

	}
	return 0;
}

/*
 * update_replicas_linkage -- (internal) update uuids linking replicas
 */
static int
update_replicas_linkage(struct pool_set *set, unsigned repn)
{
	struct pool_replica *rep = REP(set, repn);
	struct pool_replica *prev_r = REP(set, repn - 1);
	struct pool_replica *next_r = REP(set, repn + 1);

	/* set uuids in the current replica */
	for (unsigned p = 0; p < rep->nparts; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->prev_repl_uuid, PART(prev_r, 0).uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_repl_uuid, PART(next_r, 0).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(hdrp, sizeof(*hdrp));

	}

	/* set uuids in the previous replica */
	for (unsigned p = 0; p < prev_r->nparts; ++p) {
		struct pool_hdr *prev_hdrp = HDR(prev_r, p);
		memcpy(prev_hdrp->next_repl_uuid, PART(rep, 0).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(prev_hdrp, sizeof(*prev_hdrp),
				&prev_hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(prev_hdrp, sizeof(*prev_hdrp));
	}

	/* set uuids in the next replica */
	for (unsigned p = 0; p < next_r->nparts; ++p) {
		struct pool_hdr *next_hdrp = HDR(next_r, p);

		memcpy(next_hdrp->prev_repl_uuid, PART(rep, 0).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(next_hdrp, sizeof(*next_hdrp),
				&next_hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(next_hdrp, sizeof(*next_hdrp));
	}

	return 0;
}

/*
 * update_poolset_uuids -- (internal) update poolset uuid in recreated parts
 */
static int
update_poolset_uuids(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs)
{
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nparts; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->poolset_uuid, set->uuid, POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1);

		/* store pool's header */
		pmem_msync(hdrp, sizeof(*hdrp));
	}
	return 0;
}

/*
 * update_uuids -- (internal) set all uuids that might have changed or be unset
 *                 after recreating parts
 */
static int
update_uuids(struct pool_set *set, struct poolset_health_status *set_hs)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (replica_is_replica_broken(r, set_hs) ||
				replica_is_replica_inconsistent(r, set_hs))
			update_parts_linkage(set, r, set_hs);

		update_replicas_linkage(set, r);
		update_poolset_uuids(set, r, set_hs);
	}
	return 0;
}

/*
 * sync_replica -- synchronize data across replicas within a poolset
 */
int
sync_replica(struct pool_set *set, unsigned flags)
{
	ASSERTne(set, NULL);

	/* examine poolset's health */
	struct poolset_health_status *set_hs = NULL;
	if (replica_check_poolset_health(set, &set_hs, flags))
		return -1;

	/* check if poolset is broken; if not, nothing to do */
	if (replica_is_poolset_healthy(set_hs)) {
		LOG(2, "Poolset is healthy");
		goto OK_close;
	}

	/* find one good replica; it will be the source of data */
	unsigned healthy_replica = replica_find_healthy_replica(set_hs);
	if (healthy_replica == UNDEF_REPLICA)
		goto err;

	/* recreate broken parts */
	if (recreate_broken_parts(set, set_hs, flags))
		goto err;

	/* open all part files */
	if (replica_open_poolset_part_files(set))
		goto err;

	/* map all replicas */
	if (util_poolset_open(set))
		goto err;

	/* update uuid fields in the set structure with part headers */
	fill_struct_uuids(set, healthy_replica, set_hs);

	/* create headers for broken parts */
	if (!is_dry_run(flags)) {
		if (create_headers_for_broken_parts(set, healthy_replica,
				set_hs))
			goto err;
	}

	/* check and copy data if possible */
	if (copy_data_to_broken_parts(set, healthy_replica, flags, set_hs))
		goto err;

	/* grand permission for all created parts */
	if (grant_broken_parts_perm(set, set_hs))
		goto err;

	/* update uuids of replicas and parts */
	if (!is_dry_run(flags))
		update_uuids(set, set_hs);

OK_close:
	replica_free_poolset_health_status(set_hs);
	return 0;

err:
	replica_free_poolset_health_status(set_hs);
	return -1;
}
