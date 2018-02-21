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
#include "file.h"
#include "os.h"
#include "out.h"
#include "extent.h"
#include "os_badblock.h"
#include "badblock_poolset.h"

/* structure with paths of pool files containing bad blocks */
struct files_bbs_s {
	int n_files_bbs;	/* number of files with bad blocks */
	char ***files_bbs;	/* pointer to array of file paths */
};

/*
 * os_badblocks_check_file_cb -- (internal) callback checking bad blocks
 *                               in the given file
 */
static int
os_badblocks_check_file_cb(struct part_file *pf, void *arg)
{
	struct files_bbs_s *pfbbs = arg;

	if (pf->is_remote) /* XXX not supported yet */
		return 0;

	int ret = os_badblocks_check_file(pf->path);
	if (ret < 0)
		return -1;

	if (ret > 0) {
		LOG(1, "the pool file contains bad blocks -- %s", pf->path);

		pfbbs->n_files_bbs++;

		if (pfbbs->files_bbs) {
			/* save the path of the pool file with bad blocks */
			char **files_bbs = *(pfbbs->files_bbs);
			size_t size = (size_t)pfbbs->n_files_bbs *
						sizeof(const char *);
			files_bbs = Realloc(files_bbs, size);
			files_bbs[pfbbs->n_files_bbs - 1] = (char *)pf->path;
			*(pfbbs->files_bbs) = files_bbs;
		}
	}

	return 0;
}

/*
 * os_badblocks_check_poolset -- checks if the pool set contains bad blocks
 *                               (and optionally returns array of paths
 *                                of pool files containing bad blocks)
 */
int
os_badblocks_check_poolset(struct pool_set *set, char ***files_bbs)
{
	LOG(3, "set %p", set);

	struct files_bbs_s fbbs;

	fbbs.n_files_bbs = 0;
	fbbs.files_bbs = files_bbs;

	int ret = util_poolset_foreach_part_set(set,
						os_badblocks_check_file_cb,
						&fbbs);
	if (ret) {
		ERR("failed to check poolset file for bad blocks");
		return -1;
	}

	if (fbbs.n_files_bbs) {
		LOG(1, "%i pool file(s) contain bad blocks", fbbs.n_files_bbs);
	}

	return fbbs.n_files_bbs;
}
