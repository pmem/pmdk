// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2024, Intel Corporation */

/*
 * set_badblocks.c - common part of implementation of bad blocks API
 */
#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>

#include "file.h"
#include "os.h"
#include "out.h"
#include "set_badblocks.h"
#include "badblocks.h"

/* helper structure for badblocks_check_file_cb() */
struct check_file_cb {
	int n_files_bbs;	/* number of files with bad blocks */
	int create;		/* poolset is just being created */
};

/*
 * badblocks_check_file_cb -- (internal) callback checking bad blocks
 *                               in the given file
 */
static int
badblocks_check_file_cb(struct part_file *pf, void *arg)
{
	LOG(3, "part_file %p arg %p", pf, arg);

	struct check_file_cb *pcfcb = arg;

	int exists = util_file_exists(pf->part->path);
	if (exists < 0)
		return -1;

	if (!exists)
		/* the part does not exist, so it has no bad blocks */
		return 0;

	int ret = badblocks_check_file(pf->part->path);
	if (ret < 0) {
		ERR_WO_ERRNO(
			"checking the pool file for bad blocks failed -- '%s'",
			pf->part->path);
		return -1;
	}

	if (ret > 0) {
		ERR_WO_ERRNO("part file contains bad blocks -- '%s'",
			pf->part->path);
		pcfcb->n_files_bbs++;
		pf->part->has_bad_blocks = 1;
	}

	return 0;
}

/*
 * badblocks_check_poolset -- checks if the pool set contains bad blocks
 *
 * Return value:
 * -1 error
 *  0 pool set does not contain bad blocks
 *  1 pool set contains bad blocks
 */
int
badblocks_check_poolset(struct pool_set *set, int create)
{
	LOG(3, "set %p create %i", set, create);

	struct check_file_cb cfcb;

	cfcb.n_files_bbs = 0;
	cfcb.create = create;

	if (util_poolset_foreach_part_struct(set, badblocks_check_file_cb,
						&cfcb)) {
		return -1;
	}

	if (cfcb.n_files_bbs) {
		CORE_LOG_ERROR("%i pool file(s) contain bad blocks",
			cfcb.n_files_bbs);
		set->has_bad_blocks = 1;
	}

	return (cfcb.n_files_bbs > 0);
}

/*
 * badblocks_clear_poolset_cb -- (internal) callback clearing bad blocks
 *                                  in the given file
 */
static int
badblocks_clear_poolset_cb(struct part_file *pf, void *arg)
{
	LOG(3, "part_file %p arg %p", pf, arg);

	int *create = arg;

	if (*create) {
		/*
		 * Poolset is just being created - check if file exists
		 * and if we can read it.
		 */
		int exists = util_file_exists(pf->part->path);
		if (exists < 0)
			return -1;

		if (!exists)
			return 0;
	}

	int ret = badblocks_clear_all(pf->part->path);
	if (ret < 0) {
		ERR_WO_ERRNO(
			"clearing bad blocks in the pool file failed -- '%s'",
			pf->part->path);
		errno = EIO;
		return -1;
	}

	pf->part->has_bad_blocks = 0;

	return 0;
}

/*
 * badblocks_clear_poolset -- clears bad blocks in the pool set
 */
int
badblocks_clear_poolset(struct pool_set *set, int create)
{
	LOG(3, "set %p create %i", set, create);

	if (util_poolset_foreach_part_struct(set, badblocks_clear_poolset_cb,
						&create)) {
		return -1;
	}

	set->has_bad_blocks = 0;

	return 0;
}

/*
 * badblocks_recovery_file_alloc -- allocate name of bad block recovery file,
 *                                  the allocated name has to be freed
 *                                  using Free()
 */
char *
badblocks_recovery_file_alloc(const char *file, unsigned rep, unsigned part)
{
	LOG(3, "file %s rep %u part %u", file, rep, part);

	char bbs_suffix[64];
	char *path;

	sprintf(bbs_suffix, "_r%u_p%u_badblocks.txt", rep, part);

	size_t len_file = strlen(file);
	size_t len_bbs_suffix = strlen(bbs_suffix);
	size_t len_path = len_file + len_bbs_suffix;

	path = Malloc(len_path + 1);
	if (path == NULL) {
		ERR_W_ERRNO("Malloc");
		return NULL;
	}

	strcpy(path, file);
	strcat(path, bbs_suffix);

	return path;
}

/*
 * badblocks_recovery_file_exists -- check if any bad block recovery file exists
 *
 * Returns:
 *    0 when there are no bad block recovery files and
 *    1 when there is at least one bad block recovery file.
 */
int
badblocks_recovery_file_exists(struct pool_set *set)
{
	LOG(3, "set %p", set);

	int recovery_file_exists = 0;

	for (unsigned r = 0; r < set->nreplicas; ++r) {
		struct pool_replica *rep = set->replica[r];

		for (unsigned p = 0; p < rep->nparts; ++p) {
			const char *path = PART(rep, p)->path;

			int exists = util_file_exists(path);
			if (exists < 0)
				return -1;

			if (!exists) {
				/* part file does not exist - skip it */
				continue;
			}

			char *rec_file =
				badblocks_recovery_file_alloc(set->path, r, p);
			if (rec_file == NULL) {
				CORE_LOG_ERROR(
					"allocating name of bad block recovery file failed");
				return -1;
			}

			exists = util_file_exists(rec_file);
			if (exists < 0) {
				Free(rec_file);
				return -1;
			}

			if (exists) {
				LOG(3, "bad block recovery file exists: %s",
					rec_file);

				recovery_file_exists = 1;
			}

			Free(rec_file);

			if (recovery_file_exists)
				return 1;
		}
	}

	return 0;
}
