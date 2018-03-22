/*
 * Copyright 2018, Intel Corporation
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
 * badblock_poolset.c - implementation of bad block API for poolsets
 */
#define _GNU_SOURCE

#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>

#include "file.h"
#include "os.h"
#include "out.h"
#include "extent.h"
#include "os_badblock.h"
#include "badblock_poolset.h"

/* helper structure for os_badblocks_check_file_cb() */
struct check_file_cb {
	int n_files_bbs;	/* number of files with bad blocks */
	int create;		/* poolset is just being created */
};

/*
 * os_badblocks_check_file_cb -- (internal) callback checking bad blocks
 *                               in the given file
 */
static int
os_badblocks_check_file_cb(struct part_file *pf, void *arg)
{
	LOG(3, "part_file %p arg %p", pf, arg);

	struct check_file_cb *pcfcb = arg;

	if (pf->is_remote) { /* XXX not supported yet */
		LOG(1,
			"WARNING: checking remote replicas for bad blocks is not supported yet -- '%s:%s'",
			pf->node_addr, pf->pool_desc);
		return 0;
	}

	if (pcfcb->create) {
		/*
		 * Poolset is just being created - check if file exists
		 * and if we can read it.
		 */
		int exists = os_access(pf->part->path, F_OK) == 0;
		if (!exists)
			return 0;
	}

	int ret = os_badblocks_check_file(pf->part->path);
	if (ret < 0) {
		ERR("checking the pool file for bad blocks failed -- '%s'",
			pf->part->path);
		return -1;
	}

	if (ret > 0) {
		LOG(1, "the pool file contains bad blocks -- '%s'",
			pf->part->path);
		pcfcb->n_files_bbs++;
		pf->part->has_bad_blocks = 1;
	}

	return 0;
}

/*
 * os_badblocks_check_poolset -- checks if the pool set contains bad blocks
 *
 * Return value:
 * -1 error
 *  0 pool set does not contain bad blocks
 *  1 pool set contains bad blocks
 */
int
os_badblocks_check_poolset(struct pool_set *set, int create)
{
	LOG(3, "set %p create %i", set, create);

	struct check_file_cb cfcb;

	cfcb.n_files_bbs = 0;
	cfcb.create = create;

	if (util_poolset_foreach_part_struct(set, os_badblocks_check_file_cb,
						&cfcb)) {
		return -1;
	}

	if (cfcb.n_files_bbs) {
		LOG(1, "%i pool file(s) contain bad blocks", cfcb.n_files_bbs);
		set->has_bad_blocks = 1;
	}

	return (cfcb.n_files_bbs > 0);
}

/*
 * os_badblocks_clear_poolset_cb -- (internal) callback clearing bad blocks
 *                                  in the given file
 */
static int
os_badblocks_clear_poolset_cb(struct part_file *pf, void *arg)
{
	LOG(3, "part_file %p arg %p", pf, arg);

	int *create = arg;

	if (pf->is_remote) { /* XXX not supported yet */
		LOG(1,
			"WARNING: clearing bad blocks in remote replicas is not supported yet -- '%s:%s'",
			pf->node_addr, pf->pool_desc);
		return 0;
	}

	if (*create) {
		/*
		 * Poolset is just being created - check if file exists
		 * and if we can read it.
		 */
		int exists = os_access(pf->part->path, F_OK) == 0;
		if (!exists)
			return 0;
	}

	int ret = os_badblocks_clear(pf->part->path);
	if (ret < 0) {
		ERR("clearing bad blocks in the pool file failed -- '%s'",
			pf->part->path);
		errno = EIO;
		return -1;
	}

	pf->part->has_bad_blocks = 0;

	return 0;
}

/*
 * os_badblocks_clear_poolset -- clears bad blocks in the pool set
 */
int
os_badblocks_clear_poolset(struct pool_set *set, int create)
{
	LOG(3, "set %p create %i", set, create);

	if (util_poolset_foreach_part_struct(set, os_badblocks_clear_poolset_cb,
						&create)) {
		return -1;
	}

	set->has_bad_blocks = 0;

	return 0;
}
