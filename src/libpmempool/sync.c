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
#include "os.h"
#include "util_pmem.h"
#include "util.h"

#ifdef USE_RPMEM
#include "rpmem_common.h"
#include "rpmem_ssh.h"
#endif

/*
 * validate_args -- (internal) check whether passed arguments are valid
 */
static int
validate_args(struct pool_set *set)
{
	LOG(3, "set %p", set);
	ASSERTne(set, NULL);

	/* the checks below help detect use of incorrect poolset file */

	/*
	 * check if all parts in the poolset are large enough
	 * (now replication works only for pmemobj pools)
	 */
	if (replica_check_part_sizes(set, PMEMOBJ_MIN_POOL)) {
		LOG(2, "part sizes check failed");
		goto err;
	}

	/*
	 * check if all directories for part files exist
	 */
	if (replica_check_part_dirs(set)) {
		LOG(2, "part directories check failed");
		goto err;
	}

	return 0;

err:
	if (errno == 0)
		errno = EINVAL;
	return -1;
}

/*
 * recreate_broken_parts -- (internal) create parts in place of the broken ones
 */
static int
recreate_broken_parts(struct pool_set *set,
	struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, set_hs %p, flags %u", set, set_hs, flags);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		if (set->replica[r]->remote)
			continue;

		struct pool_replica *broken_r = set->replica[r];

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; ++p) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs))
				continue;

			/* remove parts from broken replica */
			if (!is_dry_run(flags)) {
				if (replica_remove_part(set, r, p)) {
					LOG(2, "cannot remove part");
					return -1;
				}
			}

			/* create removed part and open it */
			if (util_part_open(&broken_r->part[p], 0,
					!is_dry_run(flags))) {
				LOG(2, "cannot open/create parts");
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
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	struct pool_hdr *hdrp;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip broken parts */
		if (replica_is_part_broken(repn, p, set_hs))
			continue;

		hdrp = HDR(rep, p);
		memcpy(rep->part[p].uuid, hdrp->uuid, POOL_HDR_UUID_LEN);
	}
}

/*
 * is_uuid_already_used -- (internal) check if given uuid is assigned to
 *                         any of the earlier replicas
 */
static int
is_uuid_already_used(uuid_t uuid, struct pool_set *set, unsigned repn)
{
	for (unsigned r = 0; r < repn; ++r) {
		if (uuidcmp(uuid, PART(REP(set, r), 0).uuid) == 0)
			return 1;
	}
	return 0;
}

/*
 * fill_struct_broken_part_uuids -- (internal) set part uuids in pool_set
 *                                  structure
 */
static int
fill_struct_broken_part_uuids(struct pool_set *set, unsigned repn,
		struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, repn %u, set_hs %p, flags %u", set, repn, set_hs,
			flags);
	struct pool_replica *rep = REP(set, repn);
	struct pool_hdr *hdrp;
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		/* skip unbroken parts */
		if (!replica_is_part_broken(repn, p, set_hs))
			continue;

		/* check if part was damaged or was added by transform */
		if (replica_is_poolset_transformed(flags)) {
			/* generate new uuid for this part */
			if (util_uuid_generate(rep->part[p].uuid) < 0) {
				ERR("cannot generate pool set part UUID");
				errno = EINVAL;
				return -1;
			}
			continue;
		}

		if (!replica_is_part_broken(repn, p - 1, set_hs) &&
				!(set->options & OPTION_SINGLEHDR)) {
			/* try to get part uuid from the previous part */
			hdrp = HDRP(rep, p);
			memcpy(rep->part[p].uuid, hdrp->next_part_uuid,
					POOL_HDR_UUID_LEN);
		} else if (!replica_is_part_broken(repn, p + 1, set_hs) &&
				!(set->options & OPTION_SINGLEHDR)) {
			/* try to get part uuid from the next part */
			hdrp = HDRN(rep, p);
			memcpy(rep->part[p].uuid, hdrp->prev_part_uuid,
					POOL_HDR_UUID_LEN);
		} else if (p == 0 &&
			!replica_is_part_broken(repn - 1, 0, set_hs)) {
			/* try to get part uuid from the previous replica */
			hdrp = HDR(REPP(set, repn), 0);
			if (is_uuid_already_used(hdrp->next_repl_uuid, set,
					repn)) {
				ERR("repeated uuid - some replicas were created"
					" with a different poolset file");
				errno = EINVAL;
				return -1;
			}
			memcpy(rep->part[p].uuid, hdrp->next_repl_uuid,
						POOL_HDR_UUID_LEN);
		} else if (p == 0 &&
			!replica_is_part_broken(repn + 1, 0, set_hs)) {
			/* try to get part uuid from the next replica */
			hdrp = HDR(REPN(set, repn), 0);
			if (is_uuid_already_used(hdrp->prev_repl_uuid, set,
					repn)) {
				ERR("repeated uuid - some replicas were created"
					" with a different poolset file");
				errno = EINVAL;
				return -1;
			}
			memcpy(rep->part[p].uuid, hdrp->prev_repl_uuid,
						POOL_HDR_UUID_LEN);
		} else {
			/* generate new uuid for this part */
			if (util_uuid_generate(rep->part[p].uuid) < 0) {
				ERR("cannot generate pool set part UUID");
				errno = EINVAL;
				return -1;
			}
		}
	}
	return 0;
}

/*
 * fill_struct_uuids -- (internal) fill fields in pool_set needed for further
 *                      altering of uuids
 */
static int
fill_struct_uuids(struct pool_set *set, unsigned src_replica,
		struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, src_replica %u, set_hs %p, flags %u", set, src_replica,
			set_hs, flags);

	/* set poolset uuid */
	struct pool_hdr *src_hdr0 = HDR(REP(set, src_replica), 0);
	memcpy(set->uuid, src_hdr0->poolset_uuid, POOL_HDR_UUID_LEN);

	/* set unbroken parts' uuids */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		fill_struct_part_uuids(set, r, set_hs);
	}

	/* set broken parts' uuids */
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (fill_struct_broken_part_uuids(set, r, set_hs, flags))
			return -1;
	}
	return 0;
}

/*
 * create_headers_for_broken_parts -- (internal) create headers for all new
 *                                    parts created in place of the broken ones
 */
static int
create_headers_for_broken_parts(struct pool_set *set, unsigned src_replica,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, src_replica %u, set_hs %p", set, src_replica, set_hs);
	struct pool_hdr *src_hdr = HDR(REP(set, src_replica), 0);
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken replicas */
		if (!replica_is_replica_broken(r, set_hs))
			continue;

		for (unsigned p = 0; p < set_hs->replica[r]->nhdrs; p++) {
			/* skip unbroken parts */
			if (!replica_is_part_broken(r, p, set_hs))
				continue;

			struct pool_attr attr;
			util_get_attr_from_header(&attr, src_hdr);
			//XXX: uuids + flags = NULL ?
			if (util_header_create(set, r, p, &attr, 0) != 0) {
				LOG(1, "part headers create failed for"
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
	LOG(3, "set %p, healthy_replica %u, flags %u, set_hs %p", set,
			healthy_replica, flags, set_hs);

	/* get pool size from healthy replica */
	size_t poolsize = set->poolsize;

	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken and consistent replicas */
		if (replica_is_replica_healthy(r, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);
		struct pool_replica *rep_h = REP(set, healthy_replica);

		for (unsigned p = 0; p < rep->nparts; ++p) {
			/* skip unbroken parts from consistent replicas */
			if (!replica_is_part_broken(r, p, set_hs) &&
				replica_is_replica_consistent(r, set_hs))
				continue;

			const struct pool_set_part *part = &rep->part[p];

			size_t off = replica_get_part_data_offset(set, r, p);
			size_t len = replica_get_part_data_len(set, r, p);

			/* do not allow copying too much data */
			if (off >= poolsize)
				continue;

			if (off + len > poolsize || rep->remote)
				len = poolsize - off;

			/*
			 * First part of replica is mapped
			 * with header
			 */
			size_t fpoff = (p == 0) ? POOL_HDR_SIZE : 0;
			void *dst_addr = ADDR_SUM(part->addr, fpoff);

			if (rep->remote) {
				int ret = Rpmem_persist(rep->remote->rpp, off,
						len, 0);
				if (ret) {
					LOG(1, "Copying data to remote node "
						"failed -- '%s' on '%s'",
						rep->remote->pool_desc,
						rep->remote->node_addr);
					return -1;
				}
			} else if (rep_h->remote) {
				int ret = Rpmem_read(rep_h->remote->rpp,
						dst_addr, off, len, 0);
				if (ret) {
					LOG(1, "Reading data from remote node "
						"failed -- '%s' on '%s'",
						rep_h->remote->pool_desc,
						rep_h->remote->node_addr);
					return -1;
				}
			} else {
				if (off + len > poolsize)
					len = poolsize - off;

				void *src_addr =
					ADDR_SUM(rep_h->part[0].addr, off);

				/* copy all data */
				memcpy(dst_addr, src_addr, len);
				util_persist(part->is_dev_dax, dst_addr, len);
			}
		}
	}
	return 0;
}

/*
 * grant_created_parts_perm -- (internal) set RW permission rights to all
 *                            the parts created in place of the broken ones
 */
static int
grant_created_parts_perm(struct pool_set *set, unsigned src_repn,
		struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, src_repn %u, set_hs %p", set, src_repn, set_hs);

	/* choose the default permissions */
	mode_t def_mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP;

	/* get permissions of the first part of the source replica */
	mode_t src_mode;
	os_stat_t sb;
	if (os_stat(PART(REP(set, src_repn), 0).path, &sb) != 0) {
		ERR("cannot check file permissions of %s (replica %u, part %u)",
				PART(REP(set, src_repn), 0).path, src_repn, 0);
		src_mode = def_mode;
	} else {
		src_mode = sb.st_mode;
	}

	/* set permissions to all recreated parts */
	for (unsigned r = 0; r < set_hs->nreplicas; ++r) {
		/* skip unbroken replicas */
		if (!replica_is_replica_broken(r, set_hs))
			continue;

		if (set->replica[r]->remote)
			continue;

		for (unsigned p = 0; p < set_hs->replica[r]->nparts; p++) {
			/* skip parts which were not created */
			if (!PART(REP(set, r), p).created)
				continue;

			LOG(4, "setting permissions for part %u, replica %u",
					p, r);

			/* set rights to those of existing part files */
			if (os_chmod(PART(REP(set, r), p).path, src_mode)) {
				ERR("cannot set permission rights for created"
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
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		struct pool_hdr *prev_hdrp = HDRP(rep, p);
		struct pool_hdr *next_hdrp = HDRN(rep, p);

		/* set uuids in the current part */
		memcpy(hdrp->prev_part_uuid, PARTP(rep, p).uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_part_uuid, PARTN(rep, p).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF);

		/* set uuids in the previous part */
		memcpy(prev_hdrp->next_part_uuid, PART(rep, p).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(prev_hdrp, sizeof(*prev_hdrp),
			&prev_hdrp->checksum, 1, POOL_HDR_CSUM_END_OFF);

		/* set uuids in the next part */
		memcpy(next_hdrp->prev_part_uuid, PART(rep, p).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(next_hdrp, sizeof(*next_hdrp),
			&next_hdrp->checksum, 1, POOL_HDR_CSUM_END_OFF);

		/* store pool's header */
		util_persist(PART(rep, p).is_dev_dax, hdrp, sizeof(*hdrp));
		util_persist(PARTP(rep, p).is_dev_dax, prev_hdrp,
				sizeof(*prev_hdrp));
		util_persist(PARTN(rep, p).is_dev_dax, next_hdrp,
				sizeof(*next_hdrp));

	}
	return 0;
}

/*
 * update_replicas_linkage -- (internal) update uuids linking replicas
 */
static int
update_replicas_linkage(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = REP(set, repn);
	struct pool_replica *prev_r = REPP(set, repn);
	struct pool_replica *next_r = REPN(set, repn);

	ASSERT(rep->nparts > 0);
	ASSERT(prev_r->nparts > 0);
	ASSERT(next_r->nparts > 0);

	/* set uuids in the current replica */
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->prev_repl_uuid, PART(prev_r, 0).uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->next_repl_uuid, PART(next_r, 0).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF);

		/* store pool's header */
		util_persist(PART(rep, p).is_dev_dax, hdrp, sizeof(*hdrp));
	}

	/* set uuids in the previous replica */
	for (unsigned p = 0; p < prev_r->nhdrs; ++p) {
		struct pool_hdr *prev_hdrp = HDR(prev_r, p);
		memcpy(prev_hdrp->next_repl_uuid, PART(rep, 0).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(prev_hdrp, sizeof(*prev_hdrp),
			&prev_hdrp->checksum, 1, POOL_HDR_CSUM_END_OFF);

		/* store pool's header */
		util_persist(PART(prev_r, p).is_dev_dax, prev_hdrp,
				sizeof(*prev_hdrp));
	}

	/* set uuids in the next replica */
	for (unsigned p = 0; p < next_r->nhdrs; ++p) {
		struct pool_hdr *next_hdrp = HDR(next_r, p);

		memcpy(next_hdrp->prev_repl_uuid, PART(rep, 0).uuid,
				POOL_HDR_UUID_LEN);
		util_checksum(next_hdrp, sizeof(*next_hdrp),
			&next_hdrp->checksum, 1, POOL_HDR_CSUM_END_OFF);

		/* store pool's header */
		util_persist(PART(next_r, p).is_dev_dax, next_hdrp,
				sizeof(*next_hdrp));
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
	LOG(3, "set %p, repn %u, set_hs %p", set, repn, set_hs);
	struct pool_replica *rep = REP(set, repn);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->poolset_uuid, set->uuid, POOL_HDR_UUID_LEN);
		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum,
			1, POOL_HDR_CSUM_END_OFF);

		/* store pool's header */
		util_persist(PART(rep, p).is_dev_dax, hdrp, sizeof(*hdrp));
	}
	return 0;
}

/*
 * update_remote_headers -- (internal) update headers of existing remote
 *                          replicas
 */
static int
update_remote_headers(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++ r) {
		/* skip local or just created replicas */
		if (REP(set, r)->remote == NULL ||
				PART(REP(set, r), 0).created == 1)
			continue;

		//XXX
		if (util_update_remote_header(set, r)) {
			LOG(1, "updating header of a remote replica no. %u"
					" failed", r);
			return -1;
		}
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
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!replica_is_replica_healthy(r, set_hs))
			update_parts_linkage(set, r, set_hs);

		update_replicas_linkage(set, r);
		update_poolset_uuids(set, r, set_hs);
	}

	if (update_remote_headers(set))
		return -1;

	return 0;
}

/*
 * remove_remote -- (internal) remove remote pool
 */
static int
remove_remote(const char *target, const char *pool_set)
{
	LOG(3, "target %s, pool_set %s", target, pool_set);
#ifdef USE_RPMEM
	struct rpmem_target_info *info = rpmem_target_parse(target);
	if (!info)
		goto err_parse;

	struct rpmem_ssh *ssh = rpmem_ssh_exec(info, "--remove",
			pool_set, "--force", NULL);
	if (!ssh) {
		goto err_ssh_exec;
	}

	if (rpmem_ssh_monitor(ssh, 0))
		goto err_ssh_monitor;

	int ret = rpmem_ssh_close(ssh);
	rpmem_target_free(info);

	return ret;
err_ssh_monitor:
	rpmem_ssh_close(ssh);
err_ssh_exec:
	rpmem_target_free(info);
err_parse:
	return -1;
#else
	FATAL("remote replication not supported");
	return -1;
#endif
}

/*
 * open_remote_replicas -- (internal) open all unbroken remote replicas
 */
static int
open_remote_replicas(struct pool_set *set,
	struct poolset_health_status *set_hs)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		if (!rep->remote)
			continue;
		if (!replica_is_replica_healthy(r, set_hs))
			continue;

		unsigned nlanes = REMOTE_NLANES;
		int ret = util_poolset_remote_replica_open(set, r,
				set->poolsize, 0, &nlanes);
		if (ret) {
			LOG(1, "Opening '%s' on '%s' failed",
					rep->remote->pool_desc,
					rep->remote->node_addr);
			return ret;
		}
	}

	return 0;
}

/*
 * create_remote_replicas -- (internal) recreate all broken replicas
 */
static int
create_remote_replicas(struct pool_set *set,
	struct poolset_health_status *set_hs, unsigned flags)
{
	LOG(3, "set %p, set_hs %p", set, set_hs);
	for (unsigned r = 0; r < set->nreplicas; r++) {
		struct pool_replica *rep = set->replica[r];
		if (!rep->remote)
			continue;
		if (replica_is_replica_healthy(r, set_hs))
			continue;

		if (!replica_is_poolset_transformed(flags)) {
			/* ignore errors from remove operation */
			remove_remote(rep->remote->node_addr,
					rep->remote->pool_desc);
		}

		unsigned nlanes = REMOTE_NLANES;
		int ret = util_poolset_remote_replica_open(set, r,
				set->poolsize, 1, &nlanes);
		if (ret) {
			LOG(1, "Creating '%s' on '%s' failed",
					rep->remote->pool_desc,
					rep->remote->node_addr);
			return ret;
		}
	}

	return 0;
}


/*
 * sync_replica -- synchronize data across replicas within a poolset
 */
int
replica_sync(struct pool_set *set, struct poolset_health_status *s_hs,
		unsigned flags)
{
	LOG(3, "set %p, flags %u", set, flags);
	int ret = 0;
	struct poolset_health_status *set_hs = NULL;

	/* check if we already know the poolset health status */
	if (s_hs == NULL) {
		/* validate poolset before checking its health */
		if (validate_args(set))
			return -1;

		/* examine poolset's health */
		if (replica_check_poolset_health(set, &set_hs, flags)) {
			ERR("poolset health check failed");
			return -1;
		}

		/* check if poolset is broken; if not, nothing to do */
		if (replica_is_poolset_healthy(set_hs)) {
			LOG(1, "Poolset is healthy");
			goto out;
		}
	} else {
		set_hs = s_hs;
	}

	/* find one good replica; it will be the source of data */
	unsigned healthy_replica = replica_find_healthy_replica(set_hs);
	if (healthy_replica == UNDEF_REPLICA) {
		ERR("no healthy replica found");
		errno = EINVAL;
		ret = -1;
		goto out;
	}

	/* in dry-run mode we can stop here */
	if (is_dry_run(flags)) {
		LOG(1, "Sync in dry-run mode finished successfully");
		goto out;
	}

	/* recreate broken parts */
	if (recreate_broken_parts(set, set_hs, flags)) {
		ERR("recreating broken parts failed");
		ret = -1;
		goto out;
	}

	/* open all part files */
	if (replica_open_poolset_part_files(set)) {
		ERR("opening poolset part files failed");
		ret = -1;
		goto out;
	}

	/* map all replicas */
	if (util_poolset_open(set)) {
		ERR("opening poolset failed");
		ret = -1;
		goto out;
	}

	/* this is required for opening remote pools */
	set->poolsize = set_hs->replica[healthy_replica]->pool_size;

	/* open all remote replicas */
	if (open_remote_replicas(set, set_hs)) {
		ERR("opening remote replicas failed");
		ret = -1;
		goto out;
	}

	/* update uuid fields in the set structure with part headers */
	if (fill_struct_uuids(set, healthy_replica, set_hs, flags)) {
		ERR("gathering uuids failed");
		ret = -1;
		goto out;
	}

	/* create headers for broken parts */
	if (!is_dry_run(flags)) {
		if (create_headers_for_broken_parts(set, healthy_replica,
				set_hs)) {
			ERR("creating headers for broken parts failed");
			ret = -1;
			goto out;
		}
	}

	if (is_dry_run(flags))
		goto out;

	/* create all remote replicas */
	if (create_remote_replicas(set, set_hs, flags)) {
		ERR("creating remote replicas failed");
		ret = -1;
		goto out;
	}

	/* check and copy data if possible */
	if (copy_data_to_broken_parts(set, healthy_replica,
			flags, set_hs)) {
		ERR("copying data to broken parts failed");
		ret = -1;
		goto out;
	}

	/* update uuids of replicas and parts */
	if (update_uuids(set, set_hs)) {
		ERR("updating uuids failed");
		ret = -1;
		goto out;
	}

	/* grant permissions to all created parts */
	if (grant_created_parts_perm(set, healthy_replica, set_hs)) {
		ERR("granting permissions to created parts failed");
		ret = -1;
	}

out:
	if (s_hs == NULL)
		replica_free_poolset_health_status(set_hs);
	return ret;
}
