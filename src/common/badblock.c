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
 * badblock.c - implementation of the linux bad block API
 */

#define _GNU_SOURCE
#include <fcntl.h>

#include "file.h"
#include "os.h"
#include "out.h"
#include "extent.h"
#include "badblock.h"
#include "os_dimm.h"

#ifndef _WIN32
#include "badblock_poolset.h"
#endif

/*
 * os_badblocks_extents_common -- (internal) common operations for bad blocks:
 *                                returns list of bad blocks and extents
 *                                for the given file
 *                                (and optionally also its block size)
 */
static int
os_badblocks_extents_common(const char *file,
				struct badblocks **bbs,
				struct fiemap **fmap,
				long *blksize)
{
	ASSERTne(bbs, NULL);
	ASSERTne(fmap, NULL);

	*bbs = os_dimm_files_namespace_badblocks(file);
	if (*bbs == NULL)
		return 0;

	if ((*bbs)->bbc == 0) {
		*fmap = NULL;
		return 0;
	}

	*fmap = os_extents_get(file, blksize);
	if (*fmap == NULL) {
		Free(*bbs);
		return -1;
	}

	return 0;
}

/*
 * os_badblocks_check_file -- check if the file contains bad blocks
 *
 * Return value:
 * -1 : an error
 *  0 : no bad blocks
 *  1 : bad blocks detected
 */
int
os_badblocks_check_file(const char *file)
{
	LOG(3, "file %s", file);

	long bbsc = os_badblocks_count(file);

	if (bbsc < 0) {
		return -1;
	}

	if (bbsc > 0) {
		LOG(4, "pool file '%s' contains %li bad block(s)", file, bbsc);
		return 1;
	}

	return 0;
}

/*
 * os_badblocks_count -- returns number of bad blocks in the file
 *                       or -1 in case of an error
 */
long
os_badblocks_count(const char *file)
{
	LOG(3, "file %s", file);

	unsigned long long bb_beg, bb_end;
	unsigned long long fe_beg, fe_end;

	struct badblocks *bbs = NULL;
	struct fiemap *fmap = NULL;
	long bb_found;

	if (os_badblocks_extents_common(file, &bbs, &fmap, NULL)) {
		bb_found = -1; /* an error */
		goto exit_free_all;
	}

	bb_found = 0;

	if (bbs == NULL) {
		goto exit_free_all;
	}

	if (bbs->bbc == 0 || fmap->fm_extent_count == 0) {
		bb_found = bbs->bbc;
		goto exit_free_all;
	}

	unsigned b, e;
	for (b = 0; b < bbs->bbc; b++)
		for (e = 0; e < fmap->fm_extent_count; e++) {
			bb_beg = bbs->bbv[b].offset;
			bb_end = bb_beg + bbs->bbv[b].length - 1;
			fe_beg = fmap->fm_extents[e].fe_physical;
			fe_end = fe_beg + fmap->fm_extents[e].fe_length - 1;

			if (bb_beg <= fe_end && fe_beg <= bb_end)
				bb_found++;
		}

exit_free_all:
	if (fmap)
		Free(fmap);

	if (bbs)
		Free(bbs);

	return bb_found;
}

/*
 * os_badblocks_get -- returns list of bad blocks in the file
 */
struct badblocks *
os_badblocks_get(const char *file)
{
	LOG(3, "file %s", file);

	unsigned long long bb_beg, bb_end;
	unsigned long long fe_beg, fe_end;
	unsigned long long beg, end;

	struct badblocks *bbs = NULL;
	struct fiemap *fmap = NULL;

	struct onebadblock *bbvp = NULL;
	unsigned bb_count = 0;

	if (os_badblocks_extents_common(file, &bbs, &fmap, NULL))
		goto exit_free_all;

	if (bbs == NULL || bbs->bbc == 0 || fmap->fm_extent_count == 0)
		goto exit_free_all;

	unsigned b, e;
	for (b = 0; b < bbs->bbc; b++)
		for (e = 0; e < fmap->fm_extent_count; e++) {
			bb_beg = bbs->bbv[b].offset;
			bb_end = bb_beg + bbs->bbv[b].length - 1;
			fe_beg = fmap->fm_extents[e].fe_physical;
			fe_end = fe_beg + fmap->fm_extents[e].fe_length - 1;

			if (bb_beg > fe_end || fe_beg > bb_end)
				continue;

			beg = (bb_beg > fe_beg) ? bb_beg : fe_beg;
			end = (bb_end < fe_end) ? bb_end : fe_end;

			bbvp = Realloc(bbvp,
				(++bb_count) * sizeof(struct onebadblock));

			bbvp[bb_count - 1].length = (unsigned)(end - beg + 1);
			bbvp[bb_count - 1].offset =
					beg + fmap->fm_extents[e].fe_logical
					- fmap->fm_extents[e].fe_physical;
		}

	if (bbs->bbv)
		Free(bbs->bbv);

	bbs->bbc = bb_count;
	bbs->bbv = bbvp;

exit_free_all:
	if (fmap)
		Free(fmap);

	LOG(10, "bad blocks detected: %u", bbs ? bbs->bbc : 0);

	return bbs;
}

/*
 * os_badblocks_clear_file -- clears bad blocks in the regular file
 *                            (not in a dax device)
 */
static int
os_badblocks_clear_file(const char *file)
{
	LOG(3, "file %s", file);

	unsigned long long bb_beg, bb_end;
	unsigned long long fe_beg, fe_end;
	off_t beg, end, off, len, not_block_aligned;

	struct badblocks *bbs = NULL;
	struct fiemap *fmap = NULL;
	long blksize;
	int fd;

	if ((fd = os_open(file, O_RDWR)) < 0)
		return -1;

	if (os_badblocks_extents_common(file, &bbs, &fmap, &blksize)) {
		close(fd);
		return -1;
	}

	if (bbs->bbc == 0 || fmap->fm_extent_count == 0)
		goto exit_free_all;

	unsigned b, e;
	for (b = 0; b < bbs->bbc; b++)
		for (e = 0; e < fmap->fm_extent_count; e++) {
			bb_beg = bbs->bbv[b].offset;
			bb_end = bb_beg + bbs->bbv[b].length - 1;

			fe_beg = fmap->fm_extents[e].fe_physical;
			fe_end = fe_beg + fmap->fm_extents[e].fe_length - 1;

			if (bb_beg > fe_end || fe_beg > bb_end)
				continue;

			beg = (off_t)((bb_beg > fe_beg) ? bb_beg : fe_beg);
			end = (off_t)((bb_end < fe_end) ? bb_end : fe_end);

			len = end - beg + 1;
			off = beg + (off_t)(fmap->fm_extents[e].fe_logical
					- fmap->fm_extents[e].fe_physical);

			/* check if off is block-aligned */
			not_block_aligned = off & (blksize - 1);
			if (not_block_aligned) {
				beg -= not_block_aligned;
				off -= not_block_aligned;
				len += not_block_aligned;
			}

			/* check if len is block-aligned */
			not_block_aligned = len & (blksize - 1);
			if (not_block_aligned) {
				len += blksize - not_block_aligned;
			}

			LOG(10,
				"clearing bad block: physical offset %lu logical offset %li length %li (sectors)",
				beg >> 9, off >> 9, len >> 9);

			/* deallocate bad blocks */
			if (fallocate(fd, FALLOC_FL_PUNCH_HOLE |
					FALLOC_FL_KEEP_SIZE, off, len)) {
				perror("fallocate");
			}

			/* allocate new blocks */
			if (fallocate(fd, FALLOC_FL_KEEP_SIZE, off, len)) {
				perror("fallocate");
			}
		}

exit_free_all:
	if (fmap)
		Free(fmap);

	if (bbs)
		Free(bbs);

	close(fd);

	return 0;
}

/*
 * os_badblocks_clear -- clears bad blocks in a file
 *                      (regular file or dax device)
 */
int
os_badblocks_clear(const char *file)
{
	LOG(3, "file %s", file);

	if (util_file_is_device_dax(file))
		return os_dimm_badblocks_clear_devdax(file);

	return os_badblocks_clear_file(file);

}
