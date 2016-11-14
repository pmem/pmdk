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
 * transform.c -- a module for poolset transforming
 */

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <libgen.h>
#include <dirent.h>

#include "replica.h"
#include "out.h"
#include "obj.h"
#include "file.h"


/*
 * poolset_compare_status - a helping structure for gathering corresponding
 *                          replica numbers when comparing poolsets
 */
struct poolset_compare_status {
	unsigned nreplicas;
	unsigned flags;
	unsigned replica[];
};

/*
 * check_part_sizes -- (internal) check if all parts are large enough
 */
static int
check_part_sizes(struct pool_set *set, size_t min_size)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		for (unsigned p = 0; p < rep->nparts; ++p) {
			if (PART(rep, p).filesize < min_size) {
				ERR("some part files are too small");
				return -1;
			}
		}
	}
	return 0;
}

/*
 * check_part_dirs -- (internal) check if directories for part files exist
 */
static int
check_part_dirs(struct pool_set *set)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];
		for (unsigned p = 0; p < rep->nparts; ++p) {
			char *path = strdup(PART(rep, p).path);
			const char *dir = dirname(path);
			struct stat sb;
			if (stat(dir, &sb) != 0 || !(sb.st_mode & S_IFDIR)) {
				ERR("a directory %s for part %u in replica %u"
					" does not exist or is not accessible",
					path, p, r);
				free(path);
				return -1;
			}
			free(path);
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
	/*
	 * check if all parts in the target poolset are large enough
	 * (now replication works only for pmemobj pools)
	 */
	if (check_part_sizes(set_out, PMEMOBJ_MIN_POOL)) {
		ERR("part sizes check failed");
		goto err;
	}

	/*
	 * check if all directories for part files exist
	 */
	if (check_part_dirs(set_out)) {
		ERR("part directories check failed");
		goto err;
	}

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
	LOG(2, "p1->path: %s, p1->filesize: %lu", p1->path, p1->filesize);
	LOG(2, "p2->path: %s, p2->filesize: %lu", p2->path, p2->filesize);
	return strcmp(p1->path, p2->path) || (p1->filesize != p2->filesize);
}

/*
 * compare_replicas -- (internal) check if two replicas can be considered
 *                     the same
 */
static int
compare_replicas(struct pool_replica *r1, struct pool_replica *r2)
{
	int result = 0;
	LOG(2, "r1->nparts: %u, r2->nparts: %u", r1->nparts, r2->nparts);
	if ((result = (r1->nparts != r2->nparts)))
		return result;

	for (unsigned p = 0; p < r1->nparts; ++p)
		if ((result = compare_parts(&r1->part[p], &r2->part[p])))
			return result;

	return result;
}

/*
 * check_compare_poolsets_status -- (internal) find different replicas between
 *                                  two poolsets; for each replica which has
 *                                  a counterpart in the other poolset store
 *                                  the other replica's number in a helping
 *                                  structure
 */
static int
check_compare_poolsets_status(struct pool_set *set_in, struct pool_set *set_out,
		struct poolset_compare_status *set_in_s,
		struct poolset_compare_status *set_out_s)
{
	for (unsigned ri = 0; ri < set_in->nreplicas; ++ri) {
		struct pool_replica *rep_in = REP(set_in, ri);
		for (unsigned ro = 0; ro < set_out->nreplicas; ++ro) {
			struct pool_replica *rep_out = REP(set_out, ro);
			LOG(1, "Comparing rep_in %u with rep_out %u", ri, ro);
			/* skip different replicas */
			if (compare_replicas(rep_in, rep_out))
				continue;

			if (set_in_s->replica[ri] != UNDEF_REPLICA ||
				set_out_s->replica[ro] != UNDEF_REPLICA) {
			/* i.e. if there are more than one counterparts */
				ERR("there are more then one corresponding"
						" replicas");
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
	if (create_poolset_compare_status(set_in, set_in_s))
		return -1;

	if (create_poolset_compare_status(set_out, set_out_s))
		goto err1;

	if (check_compare_poolsets_status(set_in, set_out, *set_in_s,
			*set_out_s))
		goto err2;

	return 0;

err2:
	Free(*set_out_s);

err1:
	Free(*set_in_s);
	return -1;
}

/*
 * has_counterpart -- (internal) check if replica has a counterpart
 *                    set in the helping status structure
 */
static int
has_counterpart(unsigned repn, struct poolset_compare_status *set_s)
{
	return set_s->replica[repn] != UNDEF_REPLICA;
}

/*
 * are_poolsets_transformable -- (internal) check if poolsets can be transformed
 *                               one into the other
 */
static int
are_poolsets_transformable(struct poolset_compare_status *set_in_s,
		struct poolset_compare_status *set_out_s)
{
	for (unsigned r = 0; r < set_in_s->nreplicas; ++r) {
		if (has_counterpart(r, set_in_s)) {
			LOG(2, "Replica %u has a counterpart %u", r,
					set_in_s->replica[r]);
			return 1;
		} else {
			LOG(2, "Replica %u has no counterpar", r);
		}
	}
	return 0;
}

/*
 * delete_replica_local -- (internal) delete all part files from a given
 *                         replica
 */
static int
delete_replica_local(struct pool_set *set, unsigned repn)
{
	struct pool_replica *rep = set->replica[repn];
	for (unsigned p = 0; p < rep->nparts; ++p)
		replica_remove_part(set, repn, p);

	return 0;
}

/*
 * delete_replicas -- (internal) delete replicas which do not have their
 *                    counterpart set in the helping status structure
 */
static int
delete_replicas(struct pool_set *set, struct poolset_compare_status *set_s)
{
	for (unsigned r = 0; r < set->nreplicas; ++r) {
		if (!has_counterpart(r, set_s)) {
			util_replica_close(set, r);
			delete_replica_local(set, r);
		}
	}
	return 0;
}

/*
 * transform_replica -- transforming one poolset into another
 */
int
transform_replica(struct pool_set *set_in, struct pool_set *set_out,
		unsigned flags)
{
	/* validate user arguments */
	if (validate_args(set_in, set_out))
		return -1;

	/* examine poolset's health */
	struct poolset_compare_status *set_in_s = NULL;
	struct poolset_compare_status *set_out_s = NULL;
	if (compare_poolsets(set_in, set_out, &set_in_s, &set_out_s))
		return -1;

	if (!are_poolsets_transformable(set_in_s, set_out_s)) {
		ERR("poolsets are not transformable");
		goto err;
	}

	if (!is_dry_run(flags))
		if (delete_replicas(set_in, set_in_s))
			goto err;

	/* signal that sync is called by transform */
	flags |= IS_TRANSFORMED;
	if (sync_replica(set_out, flags))
		goto err;

	Free(set_in_s);
	Free(set_out_s);
	return 0;

err:
	Free(set_in_s);
	Free(set_out_s);
	return -1;
}
