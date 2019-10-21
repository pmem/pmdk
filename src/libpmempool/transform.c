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
 * transform.c -- a module for poolset transforming
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <dirent.h>
#include <assert.h>

#include "replica.h"
#include "out.h"
#include "file.h"
#include "os.h"
#include "libpmem.h"
#include "util_pmem.h"

/*
 * poolset_compare_status - a helping structure for gathering corresponding
 *                          replica numbers when comparing poolsets
 */
struct poolset_compare_status
{
	unsigned nreplicas;
	unsigned flags;
	unsigned replica[];
};

/*
 * type of transform operation to be done
 */
enum transform_op {
	NOT_TRANSFORMABLE,
	ADD_REPLICAS,
	RM_REPLICAS,
	ADD_HDRS,
	RM_HDRS,
};

/*
 * check_if_part_used_once -- (internal) check if the part is used only once in
 *                            the rest of the poolset
 */
static int
check_if_part_used_once(struct pool_set *set, unsigned repn, unsigned partn)
{
	LOG(3, "set %p, repn %u, partn %u", set, repn, partn);
	struct pool_replica *rep = REP(set, repn);
	char *path = util_part_realpath(PART(rep, partn)->path);
	if (path == NULL) {
		LOG(1, "cannot get absolute path for %s, replica %u, part %u",
				PART(rep, partn)->path, repn, partn);
		errno = 0;
		path = strdup(PART(rep, partn)->path);
		if (path == NULL) {
			ERR("!strdup");
			return -1;
		}
	}
	int ret = 0;
	for (unsigned r = repn; r < set->nreplicas; ++r) {
		struct pool_replica *repr = set->replica[r];
		/* skip remote replicas */
		if (repr->remote != NULL)
			continue;

		/* avoid superfluous comparisons */
		unsigned i = (r == repn) ? partn + 1 : 0;
		for (unsigned p = i; p < repr->nparts; ++p) {
			char *pathp = util_part_realpath(PART(repr, p)->path);
			if (pathp == NULL) {
				if (errno != ENOENT) {
					ERR("realpath failed for %s, errno %d",
						PART(repr, p)->path, errno);
					ret = -1;
					goto out;
				}
				LOG(1, "cannot get absolute path for %s,"
						" replica %u, part %u",
						PART(rep, partn)->path, repn,
						partn);
				pathp = strdup(PART(repr, p)->path);
				errno = 0;
			}
			int result = util_compare_file_inodes(path, pathp);
			if (result == 0) {
				/* same file used multiple times */
				ERR("some part file's path is"
						" used multiple times");
				ret = -1;
				errno = EINVAL;
				free(pathp);
				goto out;
			} else if (result < 0) {
				ERR("comparing file inodes failed for %s and"
						" %s", path, pathp);
				ret = -1;
				free(pathp);
				goto out;
			}
			free(pathp);
		}
	}
out:
	free(path);
	return ret;
}

/*
 * check_if_remote_replica_used_once -- (internal) check if remote replica is
 *                                      used only once in the rest of the
 *                                      poolset
 */
static int
check_if_remote_replica_used_once(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct remote_replica *rep = REP(set, repn)->remote;
	ASSERTne(rep, NULL);
	for (unsigned r = repn + 1; r < set->nreplicas; ++r) {
		/* skip local replicas */
		if (REP(set, r)->remote == NULL)
			continue;

		struct remote_replica *repr = REP(set, r)->remote;
		/* XXX: add comparing resolved addresses of the nodes */
		if (strcmp(rep->node_addr, repr->node_addr) == 0 &&
				strcmp(rep->pool_desc, repr->pool_desc) == 0) {
			ERR("remote replica %u is used multiple times", repn);
			errno = EINVAL;
			return -1;
		}
	}
	return 0;
}

/*
 * check_paths -- (internal) check if directories for part files exist
 *                and if paths for part files do not repeat in the poolset
 */
static int
check_paths(struct pool_set *set)
{
	LOG(3, "set %p", set);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		if (rep->remote != NULL) {
			if (check_if_remote_replica_used_once(set, r))
				return -1;
		} else {
			for (unsigned p = 0; p < rep->nparts; ++p) {
				if (replica_check_local_part_dir(set, r, p))
					return -1;

				if (check_if_part_used_once(set, r, p))
					return -1;
			}
		}
	}
	return 0;
}

/*
 * validate_args -- (internal) check whether passed arguments are valid
 */
static int
validate_args(struct pool_set *set_in, struct pool_set *set_out)
{
	LOG(3, "set_in %p, set_out %p", set_in, set_out);

	if (set_in->directory_based) {
		ERR("transform of directory poolsets is not supported");
		errno = EINVAL;
		return -1;
	}

	/*
	 * check if all parts in the target poolset are large enough
	 * (now replication works only for pmemobj pools)
	 */
	if (replica_check_part_sizes(set_out, PMEMOBJ_MIN_POOL)) {
		ERR("part sizes check failed");
		return -1;
	}

	/*
	 * check if all directories for part files exist and if part files
	 * do not reoccur in the poolset
	 */
	if (check_paths(set_out))
		return -1;

	/*
	 * check if set_out has enough size, i.e. if the target poolset
	 * structure has enough capacity to accommodate the effective size of
	 * the source poolset
	 */
	ssize_t master_pool_size = replica_get_pool_size(set_in, 0);
	if (master_pool_size < 0) {
		ERR("getting pool size from master replica failed");
		return -1;
	}

	if (set_out->poolsize < (size_t)master_pool_size) {
		ERR("target poolset is too small");
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * create poolset_compare_status -- (internal) create structure for gathering
 *                                  status of poolset comparison
 */
static int
create_poolset_compare_status(struct pool_set *set,
		struct poolset_compare_status **set_sp)
{
	LOG(3, "set %p, set_sp %p", set, set_sp);
	struct poolset_compare_status *set_s;
	set_s = Zalloc(sizeof(struct poolset_compare_status)
				+ set->nreplicas * sizeof(unsigned));
	if (set_s == NULL) {
		ERR("!Zalloc for poolset status");
		return -1;
	}
	for (unsigned r = 0; r < set->nreplicas; ++r)
		set_s->replica[r] = UNDEF_REPLICA;

	set_s->nreplicas = set->nreplicas;
	*set_sp = set_s;
	return 0;
}

/*
 * compare_parts -- (internal) check if two parts can be considered the same
 */
static int
compare_parts(struct pool_set_part *p1, struct pool_set_part *p2)
{
	LOG(3, "p1 %p, p2 %p", p1, p2);
	LOG(4, "p1->path: %s, p1->filesize: %lu", p1->path, p1->filesize);
	LOG(4, "p2->path: %s, p2->filesize: %lu", p2->path, p2->filesize);
	return strcmp(p1->path, p2->path) || (p1->filesize != p2->filesize);
}

/*
 * compare_replicas -- (internal) check if two replicas are different
 */
static int
compare_replicas(struct pool_replica *r1, struct pool_replica *r2)
{
	LOG(3, "r1 %p, r2 %p", r1, r2);
	LOG(4, "r1->nparts: %u, r2->nparts: %u", r1->nparts, r2->nparts);
	/* both replicas are local */
	if (r1->remote == NULL && r2->remote == NULL) {
		if (r1->nparts != r2->nparts)
			return 1;

		for (unsigned p = 0; p < r1->nparts; ++p) {
			if (compare_parts(&r1->part[p], &r2->part[p]))
				return 1;
		}
		return 0;
	}
	/* both replicas are remote */
	if (r1->remote != NULL && r2->remote != NULL) {
		return strcmp(r1->remote->node_addr, r2->remote->node_addr) ||
			strcmp(r1->remote->pool_desc, r2->remote->pool_desc);
	}
	/* a remote and a local replicas */
	return 1;
}

/*
 * check_compare_poolsets_status -- (internal) find different replicas between
 *                                  two poolsets; for each replica which has
 *                                  a counterpart in the other poolset store
 *                                  the other replica's number in a helping
 *                                  structure
 */
static int
check_compare_poolsets_status(struct pool_set *set_in,
		struct pool_set *set_out,
		struct poolset_compare_status *set_in_s,
		struct poolset_compare_status *set_out_s)
{
	LOG(3, "set_in %p, set_out %p, set_in_s %p, set_out_s %p", set_in,
			set_out, set_in_s, set_out_s);
	for (unsigned ri = 0; ri < set_in->nreplicas; ++ri) {
		struct pool_replica *rep_in = REP(set_in, ri);
		for (unsigned ro = 0; ro < set_out->nreplicas; ++ro) {
			struct pool_replica *rep_out = REP(set_out, ro);
			LOG(1, "comparing rep_in %u with rep_out %u", ri, ro);
			/* skip different replicas */
			if (compare_replicas(rep_in, rep_out))
				continue;

			if (set_in_s->replica[ri] != UNDEF_REPLICA ||
					set_out_s->replica[ro]
						!= UNDEF_REPLICA) {
				/* there are more than one counterparts */
				ERR("there are more then one corresponding"
						" replicas; cannot transform");
				errno = EINVAL;
				return -1;
			}

			set_in_s->replica[ri] = ro;
			set_out_s->replica[ro] = ri;
		}
	}
	return 0;
}

/*
 * check_compare_poolset_options -- (internal) check poolset options
 */
static int
check_compare_poolsets_options(struct pool_set *set_in,
		struct pool_set *set_out,
		struct poolset_compare_status *set_in_s,
		struct poolset_compare_status *set_out_s)
{
	if (set_in->options & OPTION_SINGLEHDR)
		set_in_s->flags |= OPTION_SINGLEHDR;

	if (set_out->options & OPTION_SINGLEHDR)
		set_out_s->flags |= OPTION_SINGLEHDR;

	if ((set_in->options & OPTION_NOHDRS) ||
			(set_out->options & OPTION_NOHDRS)) {
		errno = EINVAL;
		ERR(
		"the NOHDRS poolset option is not supported in local poolset files");
		return -1;
	}

	return 0;
}

/*
 * compare_poolsets -- (internal) compare two poolsets; for each replica which
 *                     has a counterpart in the other poolset store the other
 *                     replica's number in a helping structure
 */
static int
compare_poolsets(struct pool_set *set_in, struct pool_set *set_out,
		struct poolset_compare_status **set_in_s,
		struct poolset_compare_status **set_out_s)
{
	LOG(3, "set_in %p, set_out %p, set_in_s %p, set_out_s %p", set_in,
			set_out, set_in_s, set_out_s);
	if (create_poolset_compare_status(set_in, set_in_s))
		return -1;

	if (create_poolset_compare_status(set_out, set_out_s))
		goto err_free_in;

	if (check_compare_poolsets_status(set_in, set_out, *set_in_s,
			*set_out_s))
		goto err_free_out;

	if (check_compare_poolsets_options(set_in, set_out, *set_in_s,
			*set_out_s))
		goto err_free_out;

	return 0;

err_free_out:
	Free(*set_out_s);
err_free_in:
	Free(*set_in_s);
	return -1;
}

/*
 * replica_counterpart -- (internal) returns index of a counterpart replica
 */
static unsigned
replica_counterpart(unsigned repn,
		struct poolset_compare_status *set_s)
{
	return set_s->replica[repn];
}

/*
 * are_poolsets_transformable -- (internal) check if poolsets can be transformed
 *                               one into the other; also gather info about
 *                               replicas's health
 */
static enum transform_op
identify_transform_operation(struct poolset_compare_status *set_in_s,
		struct poolset_compare_status *set_out_s,
		struct poolset_health_status *set_in_hs,
		struct poolset_health_status *set_out_hs)
{
	LOG(3, "set_in_s %p, set_out_s %p", set_in_s, set_out_s);

	int has_replica_to_keep = 0;
	int is_removing_replicas = 0;
	int is_adding_replicas = 0;

	/* check if there are replicas to be removed */
	for (unsigned r = 0; r < set_in_s->nreplicas; ++r) {
		unsigned c = replica_counterpart(r, set_in_s);
		if (c != UNDEF_REPLICA) {
			LOG(2, "replica %u has a counterpart %u", r,
					set_in_s->replica[r]);
			has_replica_to_keep = 1;
			REP_HEALTH(set_out_hs, c)->pool_size =
					REP_HEALTH(set_in_hs, r)->pool_size;
		} else {
			LOG(2, "replica %u has no counterpart", r);
			is_removing_replicas = 1;
		}
	}

	/* make sure we have at least one replica to keep */
	if (!has_replica_to_keep) {
		ERR("there must be at least one replica left");
		return NOT_TRANSFORMABLE;
	}

	/* check if there are replicas to be added */
	for (unsigned r = 0; r < set_out_s->nreplicas; ++r) {
		if (replica_counterpart(r, set_out_s) == UNDEF_REPLICA) {
			LOG(2, "Replica %u from output set has no counterpart",
					r);
			if (is_removing_replicas) {
				ERR(
				"adding and removing replicas at the same time is not allowed");
				return NOT_TRANSFORMABLE;
			}

			REP_HEALTH(set_out_hs, r)->flags |= IS_BROKEN;
			is_adding_replicas = 1;
		}
	}

	/* check if there is anything to do */
	if (!is_removing_replicas && !is_adding_replicas &&
			(set_in_s->flags & OPTION_SINGLEHDR) ==
				(set_out_s->flags & OPTION_SINGLEHDR)) {
		ERR("both poolsets are equal");
		return NOT_TRANSFORMABLE;
	}

	/* allow changing the SINGLEHDR option only as the sole operation */
	if ((is_removing_replicas || is_adding_replicas) &&
			(set_in_s->flags & OPTION_SINGLEHDR) !=
				(set_out_s->flags & OPTION_SINGLEHDR)) {
		ERR(
		"cannot add/remove replicas and change the SINGLEHDR option at the same time");
		return NOT_TRANSFORMABLE;
	}

	if (is_removing_replicas)
		return RM_REPLICAS;

	if (is_adding_replicas)
		return ADD_REPLICAS;

	if (set_out_s->flags & OPTION_SINGLEHDR)
		return RM_HDRS;

	if (set_in_s->flags & OPTION_SINGLEHDR)
		return ADD_HDRS;

	ASSERT(0);
	return NOT_TRANSFORMABLE;
}

/*
 * do_added_parts_exist -- (internal) check if any part of the replicas that are
 *                         to be added (marked as broken) already exists
 */
static int
do_added_parts_exist(struct pool_set *set,
		struct poolset_health_status *set_hs)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		/* skip unbroken (i.e. not being added) replicas */
		if (!replica_is_replica_broken(r, set_hs))
			continue;

		struct pool_replica *rep = REP(set, r);

		/* skip remote replicas */
		if (rep->remote)
			continue;

		for (unsigned p = 0; p < rep->nparts; ++p) {
			/* check if part file exists */
			int oerrno = errno;
			int exists = util_file_exists(rep->part[p].path);
			if (exists < 0)
				return -1;

			if (exists && !rep->part[p].is_dev_dax) {
				LOG(1, "part file %s exists",
						rep->part[p].path);
				return 1;
			}
			errno = oerrno;
		}
	}
	return 0;
}

/*
 * delete_replicas -- (internal) delete replicas which do not have their
 *                    counterpart set in the helping status structure
 */
static int
delete_replicas(struct pool_set *set, struct poolset_compare_status *set_s)
{
	LOG(3, "set %p, set_s %p", set, set_s);
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = REP(set, r);
		if (replica_counterpart(r, set_s) == UNDEF_REPLICA) {
			if (!rep->remote) {
				if (util_replica_close_local(rep, r,
						DELETE_ALL_PARTS))
					return -1;
			} else {
				if (util_replica_close_remote(rep, r,
						DELETE_ALL_PARTS))
					return -1;
			}
		}
	}
	return 0;
}

/*
 * copy_replica_data_fw -- (internal) copy data between replicas of two
 *                         poolsets, starting from the beginning of the
 *                         second part
 */
static void
copy_replica_data_fw(struct pool_set *set_dst, struct pool_set *set_src,
		unsigned repn)
{
	LOG(3, "set_in %p, set_out %p, repn %u", set_src, set_dst, repn);
	ssize_t pool_size = replica_get_pool_size(set_src, repn);
	if (pool_size < 0) {
		LOG(1, "getting pool size from replica %u failed", repn);
		pool_size = (ssize_t)set_src->poolsize;
	}

	size_t len = (size_t)pool_size - POOL_HDR_SIZE -
			replica_get_part_data_len(set_src, repn, 0);
	void *src = PART(REP(set_src, repn), 1)->addr;
	void *dst = PART(REP(set_dst, repn), 1)->addr;
	size_t count = len / POOL_HDR_SIZE;
	while (count-- > 0) {
		pmem_memcpy_persist(dst, src, POOL_HDR_SIZE);
		src = ADDR_SUM(src, POOL_HDR_SIZE);
		dst = ADDR_SUM(dst, POOL_HDR_SIZE);
	}
}

/*
 * copy_replica_data_bw -- (internal) copy data between replicas of two
 *                         poolsets, starting from the end of the pool
 */
static void
copy_replica_data_bw(struct pool_set *set_dst, struct pool_set *set_src,
		unsigned repn)
{
	LOG(3, "set_in %p, set_out %p, repn %u", set_src, set_dst, repn);
	ssize_t pool_size = replica_get_pool_size(set_src, repn);
	if (pool_size < 0) {
		LOG(1, "getting pool size from replica %u failed", repn);
		pool_size = (ssize_t)set_src->poolsize;
	}

	size_t len = (size_t)pool_size - POOL_HDR_SIZE -
			replica_get_part_data_len(set_src, repn, 0);
	size_t count = len / POOL_HDR_SIZE;
	void *src = ADDR_SUM(PART(REP(set_src, repn), 1)->addr, len);
	void *dst = ADDR_SUM(PART(REP(set_dst, repn), 1)->addr, len);
	while (count-- > 0) {
		src = ADDR_SUM(src, -(ssize_t)POOL_HDR_SIZE);
		dst = ADDR_SUM(dst, -(ssize_t)POOL_HDR_SIZE);
		pmem_memcpy_persist(dst, src, POOL_HDR_SIZE);
	}
}

/*
 * create_missing_headers -- (internal) create headers for all parts but the
 *                           first one
 */
static int
create_missing_headers(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_hdr *src_hdr = HDR(REP(set, repn), 0);
	for (unsigned p = 1; p < set->replica[repn]->nhdrs; ++p) {
		struct pool_attr attr;
		util_pool_hdr2attr(&attr, src_hdr);
		attr.features.incompat &= (uint32_t)(~POOL_FEAT_SINGLEHDR);
		if (util_header_create(set, repn, p, &attr, 1) != 0) {
			LOG(1, "part headers create failed for"
					" replica %u part %u", repn, p);
			errno = EINVAL;
			return -1;
		}
	}
	return 0;
}

/*
 * update_replica_header -- (internal) update field values in the first header
 *                          in the replica
 */
static void
update_replica_header(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = REP(set, repn);
	struct pool_set_part *part = PART(REP(set, repn), 0);
	struct pool_hdr *hdr = (struct pool_hdr *)part->hdr;
	if (set->options & OPTION_SINGLEHDR) {
		hdr->features.incompat |= POOL_FEAT_SINGLEHDR;
		memcpy(hdr->next_part_uuid, hdr->uuid, POOL_HDR_UUID_LEN);
		memcpy(hdr->prev_part_uuid, hdr->uuid, POOL_HDR_UUID_LEN);
	} else {
		hdr->features.incompat &= (uint32_t)(~POOL_FEAT_SINGLEHDR);

	}
	util_checksum(hdr, sizeof(*hdr), &hdr->checksum, 1,
		POOL_HDR_CSUM_END_OFF(hdr));
	util_persist_auto(rep->is_pmem, hdr, sizeof(*hdr));
}

/*
 * fill_replica_struct_uuids -- (internal) gather all uuids required for the
 *                              replica in the helper structure
 */
static int
fill_replica_struct_uuids(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = REP(set, repn);
	memcpy(PART(rep, 0)->uuid, HDR(rep, 0)->uuid, POOL_HDR_UUID_LEN);
	for (unsigned p = 1; p < rep->nhdrs; ++p) {
		if (util_uuid_generate(rep->part[p].uuid) < 0) {
			ERR("cannot generate part UUID");
			errno = EINVAL;
			return -1;
		}
	}
	return 0;
}

/*
 * update_uuids -- (internal) update uuids in all headers in the replica
 */
static void
update_uuids(struct pool_set *set, unsigned repn)
{
	LOG(3, "set %p, repn %u", set, repn);
	struct pool_replica *rep = REP(set, repn);
	struct pool_hdr *hdr0 = HDR(rep, 0);
	for (unsigned p = 0; p < rep->nhdrs; ++p) {
		struct pool_hdr *hdrp = HDR(rep, p);
		memcpy(hdrp->next_part_uuid, PARTN(rep, p)->uuid,
				POOL_HDR_UUID_LEN);
		memcpy(hdrp->prev_part_uuid, PARTP(rep, p)->uuid,
				POOL_HDR_UUID_LEN);

		/* Avoid calling memcpy() on identical regions */
		if (p != 0) {
			memcpy(hdrp->next_repl_uuid, hdr0->next_repl_uuid,
					POOL_HDR_UUID_LEN);
			memcpy(hdrp->prev_repl_uuid, hdr0->prev_repl_uuid,
					POOL_HDR_UUID_LEN);
			memcpy(hdrp->poolset_uuid, hdr0->poolset_uuid,
					POOL_HDR_UUID_LEN);
		}

		util_checksum(hdrp, sizeof(*hdrp), &hdrp->checksum, 1,
			POOL_HDR_CSUM_END_OFF(hdrp));
		util_persist(PART(rep, p)->is_dev_dax, hdrp, sizeof(*hdrp));
	}
}

/*
 * copy_part_fds -- (internal) copy poolset part file descriptors between
 *                  two poolsets
 */
static void
copy_part_fds(struct pool_set *set_dst, struct pool_set *set_src)
{
	ASSERTeq(set_src->nreplicas, set_dst->nreplicas);
	for (unsigned r = 0; r < set_dst->nreplicas; ++r) {
		ASSERTeq(REP(set_src, r)->nparts, REP(set_dst, r)->nparts);
		for (unsigned p = 0; p < REP(set_dst, r)->nparts; ++p) {
			PART(REP(set_dst, r), p)->fd =
					PART(REP(set_src, r), p)->fd;
		}
	}

}

/*
 * remove_hdrs_replica -- (internal) remove headers from the replica
 */
static int
remove_hdrs_replica(struct pool_set *set_in, struct pool_set *set_out,
		unsigned repn)
{
	LOG(3, "set %p, repn %u", set_in, repn);
	int ret = 0;

	/* open all part files of the input replica */
	if (replica_open_replica_part_files(set_in, repn)) {
		LOG(1, "opening replica %u, part files failed", repn);
		ret = -1;
		goto out;
	}

	/* share part file descriptors between poolset structures */
	copy_part_fds(set_out, set_in);

	/* map the whole input replica */
	if (util_replica_open(set_in, repn, MAP_SHARED)) {
		LOG(1, "opening input replica failed: replica %u", repn);
		ret = -1;
		goto out_close;
	}

	/* map the whole output replica */
	if (util_replica_open(set_out, repn, MAP_SHARED)) {
		LOG(1, "opening output replica failed: replica %u", repn);
		ret = -1;
		goto out_unmap_in;
	}

	/* move data between the two mappings of the replica */
	if (REP(set_in, repn)->nparts > 1)
		copy_replica_data_fw(set_out, set_in, repn);

	/* make changes to the first part's header */
	update_replica_header(set_out, repn);

	util_replica_close(set_out, repn);
out_unmap_in:
	util_replica_close(set_in, repn);
out_close:
	util_replica_fdclose(REP(set_in, repn));
out:
	return ret;
}

/*
 * add_hdrs_replica -- (internal) add lacking headers to the replica
 *
 * when the operation fails and returns -1, the replica remains untouched
 */
static int
add_hdrs_replica(struct pool_set *set_in, struct pool_set *set_out,
		unsigned repn)
{
	LOG(3, "set %p, repn %u", set_in, repn);
	int ret = 0;

	/* open all part files of the input replica */
	if (replica_open_replica_part_files(set_in, repn)) {
		LOG(1, "opening replica %u, part files failed", repn);
		ret = -1;
		goto out;
	}

	/* share part file descriptors between poolset structures */
	copy_part_fds(set_out, set_in);

	/* map the whole input replica */
	if (util_replica_open(set_in, repn, MAP_SHARED)) {
		LOG(1, "opening input replica failed: replica %u", repn);
		ret = -1;
		goto out_close;
	}

	/* map the whole output replica */
	if (util_replica_open(set_out, repn, MAP_SHARED)) {
		LOG(1, "opening output replica failed: replica %u", repn);
		ret = -1;
		goto out_unmap_in;
	}

	/* generate new uuids for lacking headers */
	if (fill_replica_struct_uuids(set_out, repn)) {
		LOG(1, "generating lacking uuids for parts failed: replica %u",
				repn);
		ret = -1;
		goto out_unmap_out;
	}

	/* copy data between the two mappings of the replica */
	if (REP(set_in, repn)->nparts > 1)
		copy_replica_data_bw(set_out, set_in, repn);

	/* create the missing headers */
	if (create_missing_headers(set_out, repn)) {
		LOG(1, "creating lacking headers failed: replica %u", repn);
		/*
		 * copy the data back, so we could fall back to the original
		 * state
		 */
		if (REP(set_in, repn)->nparts > 1)
			copy_replica_data_fw(set_in, set_out, repn);
		ret = -1;
		goto out_unmap_out;
	}

	/* make changes to the first part's header */
	update_replica_header(set_out, repn);

	/* store new uuids in all headers and update linkage in the replica */
	update_uuids(set_out, repn);

out_unmap_out:
	util_replica_close(set_out, repn);
out_unmap_in:
	util_replica_close(set_in, repn);
out_close:
	util_replica_fdclose(REP(set_in, repn));
out:
	return ret;
}

/*
 * remove_hdrs -- (internal) transform a poolset without the SINGLEHDR option
 *                (with headers) into a poolset with the SINGLEHDR option
 *                (without headers)
 */
static int
remove_hdrs(struct pool_set *set_in, struct pool_set *set_out,
		struct poolset_health_status *set_in_hs, unsigned flags)
{
	LOG(3, "set_in %p, set_out %p, set_in_hs %p, flags %u",
			set_in, set_out, set_in_hs, flags);
	for (unsigned r = 0; r < set_in->nreplicas; ++r) {
		if (remove_hdrs_replica(set_in, set_out, r)) {
			LOG(1, "removing headers from replica %u failed", r);
			/* mark all previous replicas as damaged */
			while (--r < set_in->nreplicas)
				REP_HEALTH(set_in_hs, r)->flags |= IS_BROKEN;
			return -1;
		}
	}
	return 0;
}

/*
 * add_hdrs -- (internal) transform a poolset with the SINGLEHDR option (without
 *             headers) into a poolset without the SINGLEHDR option (with
 *             headers)
 */
static int
add_hdrs(struct pool_set *set_in, struct pool_set *set_out,
		struct poolset_health_status *set_in_hs,
		unsigned flags)
{
	LOG(3, "set_in %p, set_out %p, set_in_hs %p, flags %u",
			set_in, set_out, set_in_hs, flags);
	for (unsigned r = 0; r < set_in->nreplicas; ++r) {
		if (add_hdrs_replica(set_in, set_out, r)) {
			LOG(1, "adding headers to replica %u failed", r);
			/* mark all previous replicas as damaged */
			while (--r < set_in->nreplicas)
				REP_HEALTH(set_in_hs, r)->flags |= IS_BROKEN;
			return -1;
		}
	}
	return 0;
}

/*
 * transform_replica -- transforming one poolset into another
 */
int
replica_transform(struct pool_set *set_in, struct pool_set *set_out,
		unsigned flags)
{
	LOG(3, "set_in %p, set_out %p", set_in, set_out);

	int ret = 0;
	/* validate user arguments */
	if (validate_args(set_in, set_out))
		return -1;

	/* check if the source poolset is healthy */
	struct poolset_health_status *set_in_hs = NULL;
	if (replica_check_poolset_health(set_in, &set_in_hs,
					0 /* called from transform */, flags)) {
		ERR("source poolset health check failed");
		return -1;
	}

	if (!replica_is_poolset_healthy(set_in_hs)) {
		ERR("source poolset is broken");
		ret = -1;
		errno = EINVAL;
		goto free_hs_in;
	}

	/* copy value of the ignore_sds flag from the input poolset */
	set_out->ignore_sds = set_in->ignore_sds;

	struct poolset_health_status *set_out_hs = NULL;
	if (replica_create_poolset_health_status(set_out, &set_out_hs)) {
		ERR("creating poolset health status failed");
		ret = -1;
		goto free_hs_in;
	}

	/* check if the poolsets are transformable */
	struct poolset_compare_status *set_in_cs = NULL;
	struct poolset_compare_status *set_out_cs = NULL;
	if (compare_poolsets(set_in, set_out, &set_in_cs, &set_out_cs)) {
		ERR("comparing poolsets failed");
		ret = -1;
		goto free_hs_out;
	}

	enum transform_op operation = identify_transform_operation(set_in_cs,
			set_out_cs, set_in_hs, set_out_hs);

	if (operation == NOT_TRANSFORMABLE) {
		LOG(1, "poolsets are not transformable");
		ret = -1;
		errno = EINVAL;
		goto free_cs;
	}

	if (operation == RM_HDRS) {
		if (!is_dry_run(flags) &&
				remove_hdrs(set_in, set_out, set_in_hs,
						flags)) {
			ERR("removing headers failed; falling back to the "
					"input poolset");
			if (replica_sync(set_in, set_in_hs,
					flags | IS_TRANSFORMED)) {
				LOG(1, "falling back to the input poolset "
						"failed");
			} else {
				LOG(1, "falling back to the input poolset "
						"succeeded");
			}
			ret = -1;
		}
		goto free_cs;
	}

	if (operation == ADD_HDRS) {
		if (!is_dry_run(flags) &&
				add_hdrs(set_in, set_out, set_in_hs, flags)) {
			ERR("adding headers failed; falling back to the "
					"input poolset");
			if (replica_sync(set_in, set_in_hs,
					flags | IS_TRANSFORMED)) {
				LOG(1, "falling back to the input poolset "
						"failed");
			} else {
				LOG(1, "falling back to the input poolset "
						"succeeded");
			}
			ret = -1;
		}
		goto free_cs;
	}

	if (operation == ADD_REPLICAS) {
		/*
		 * check if any of the parts that are to be added already exists
		 */
		if (do_added_parts_exist(set_out, set_out_hs)) {
			ERR("some parts being added already exist");
			ret = -1;
			errno = EINVAL;
			goto free_cs;
		}
	}

	/* signal that sync is called by transform */
	if (replica_sync(set_out, set_out_hs, flags | IS_TRANSFORMED)) {
		ret = -1;
		goto free_cs;
	}

	if (operation == RM_REPLICAS) {
		if (!is_dry_run(flags) && delete_replicas(set_in, set_in_cs))
			ret = -1;
	}

free_cs:
	Free(set_in_cs);
	Free(set_out_cs);
free_hs_out:
	replica_free_poolset_health_status(set_out_hs);
free_hs_in:
	replica_free_poolset_health_status(set_in_hs);
	return ret;
}
