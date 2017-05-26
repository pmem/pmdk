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
 * check_if_part_used_once -- (internal) check if the part is used only once in
 *                            the rest of the poolset
 */
static int
check_if_part_used_once(struct pool_set *set, unsigned repn, unsigned partn)
{
	LOG(3, "set %p, repn %u, partn %u", set, repn, partn);
	struct pool_replica *rep = REP(set, repn);
	char *path = util_part_realpath(PART(rep, partn).path);
	if (path == NULL) {
		LOG(1, "cannot get absolute path for %s, replica %u, part %u",
				PART(rep, partn).path, repn, partn);
		errno = 0;
		path = strdup(PART(rep, partn).path);
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
			char *pathp = util_part_realpath(PART(repr, p).path);
			if (pathp == NULL) {
				if (errno != ENOENT) {
					ERR("realpath failed for %s, errno %d",
						PART(repr, p).path, errno);
					ret = -1;
					goto out;
				}
				LOG(1, "cannot get absolute path for %s,"
						" replica %u, part %u",
						PART(rep, partn).path, repn,
						partn);
				pathp = strdup(PART(repr, p).path);
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

	/*
	 * check if all parts in the target poolset are large enough
	 * (now replication works only for pmemobj pools)
	 */
	if (replica_check_part_sizes(set_out, PMEMOBJ_MIN_POOL)) {
		ERR("part sizes check failed");
		goto err;
	}

	/*
	 * check if all directories for part files exist and if part files
	 * do not reoccur in the poolset
	 */
	if (check_paths(set_out))
		goto err;

	/*
	 * check if set_out has enough size, i.e. if the target poolset
	 * structure has enough capacity to accommodate the effective size of
	 * the source poolset
	 */
	if (set_out->poolsize < replica_get_pool_size(set_in, 0)) {
		ERR("target poolset is too small");
		goto err;
	}

	return 0;

err:
	if (errno == 0)
		errno = EINVAL;
	return -1;
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
static int
are_poolsets_transformable(struct poolset_compare_status *set_in_s,
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
			REP(set_out_hs, c)->pool_size =
					REP(set_in_hs, r)->pool_size;
		} else {
			LOG(2, "replica %u has no counterpart", r);
			is_removing_replicas = 1;
		}
	}

	/* make sure we have at least one replica to keep */
	if (!has_replica_to_keep)
		return 0;

	/* check if there are replicas to be added */
	for (unsigned r = 0; r < set_out_s->nreplicas; ++r) {
		if (replica_counterpart(r, set_out_s) == UNDEF_REPLICA) {
			LOG(2, "Replica %u from output set has no counterpart",
					r);
			if (is_removing_replicas)
				/*
				 * adding and removing replicas at the same time
				 * is not allowed
				 */
				return 0;

			REP(set_out_hs, r)->flags |= IS_BROKEN;
			is_adding_replicas = 1;
		}
	}

	/* check if there is anything to do */
	if (!is_removing_replicas && !is_adding_replicas) {
		LOG(2, "both poolsets are equal");
		return 0;
	}
	return 1;
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
			if (os_access(rep->part[p].path, F_OK) == 0 &&
					!rep->part[p].is_dev_dax) {
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
	if (replica_check_poolset_health(set_in, &set_in_hs, flags)) {
		ERR("source poolset health check failed");
		return -1;
	}

	if (!replica_is_poolset_healthy(set_in_hs)) {
		ERR("source poolset is broken");
		ret = -1;
		goto err_free_hs_in;
	}

	struct poolset_health_status *set_out_hs = NULL;
	if (replica_create_poolset_health_status(set_out, &set_out_hs)) {
		ERR("creating poolset health status failed");
		ret = -1;
		goto err_free_hs_in;
	}

	/* check if the poolsets are transformable */
	struct poolset_compare_status *set_in_cs = NULL;
	struct poolset_compare_status *set_out_cs = NULL;
	if (compare_poolsets(set_in, set_out, &set_in_cs, &set_out_cs)) {
		ERR("comparing poolsets failed");
		ret = -1;
		goto err_free_hs_out;
	}

	if (!are_poolsets_transformable(set_in_cs, set_out_cs, set_in_hs,
			set_out_hs)) {
		ERR("poolsets are not transformable");
		ret = -1;
		errno = EINVAL;
		goto err_free_cs;
	}

	/* check if any of the parts that are to be added already exists */
	if (do_added_parts_exist(set_out, set_out_hs)) {
		ERR("some parts being added already exist");
		ret = -1;
		errno = EINVAL;
		goto err_free_cs;
	}

	/* signal that sync is called by transform */
	flags |= IS_TRANSFORMED;
	if (replica_sync(set_out, set_out_hs, flags)) {
		ret = -1;
		goto err_free_cs;
	}

	if (!is_dry_run(flags) && delete_replicas(set_in, set_in_cs))
		ret = -1;

err_free_cs:
	Free(set_in_cs);
	Free(set_out_cs);
err_free_hs_out:
	replica_free_poolset_health_status(set_out_hs);
err_free_hs_in:
	replica_free_poolset_health_status(set_in_hs);
	return ret;
}
