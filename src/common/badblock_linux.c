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
 * badblock_linux.c - implementation of the linux bad block API
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/falloc.h>

#include "file.h"
#include "os.h"
#include "out.h"
#include "extent.h"
#include "os_dimm.h"
#include "os_badblock.h"
#include "badblock.h"
#include "vec.h"

/*
 * os_badblocks_get -- returns 0 and bad blocks in the 'bbs' array
 *                     (that has to be pre-allocated)
 *                     or -1 in case of an error
 */
int
os_badblocks_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	VEC(bbsvec, struct bad_block) bbv = VEC_INITIALIZER;
	struct extents *exts = NULL;
	long extents = 0;

	unsigned long long bb_beg;
	unsigned long long bb_end;
	unsigned long long bb_len;
	unsigned long long bb_off;
	unsigned long long ext_beg;
	unsigned long long ext_end;
	unsigned long long not_block_aligned;

	int bb_found = -1; /* -1 means an error */

	memset(bbs, 0, sizeof(*bbs));

	if (os_dimm_files_namespace_badblocks(file, bbs)) {
		LOG(1, "checking the file for bad blocks failed -- '%s'", file);
		goto error_free_all;
	}

	if (bbs->bb_cnt == 0) {
		bb_found = 0;
		goto exit_free_all;
	}

	exts = Zalloc(sizeof(struct extents));
	if (exts == NULL) {
		ERR("!Zalloc");
		goto error_free_all;
	}

	extents = os_extents_count(file, exts);
	if (extents < 0) {
		LOG(1, "counting file's extents failed -- '%s'", file);
		goto error_free_all;
	}

	if (extents == 0) {
		/* dax device has no extents */
		bb_found = (int)bbs->bb_cnt;
		goto exit_free_all;
	}

	exts->extents = Zalloc(exts->extents_count * sizeof(struct extent));
	if (exts->extents == NULL) {
		ERR("!Zalloc");
		goto error_free_all;
	}

	if (os_extents_get(file, exts)) {
		LOG(1, "getting file's extents failed -- '%s'", file);
		goto error_free_all;
	}

	bb_found = 0;

	for (unsigned b = 0; b < bbs->bb_cnt; b++) {

		bb_beg = bbs->bbv[b].offset;
		bb_end = bb_beg + bbs->bbv[b].length - 1;

		for (unsigned e = 0; e < exts->extents_count; e++) {

			ext_beg = exts->extents[e].offset_physical;
			ext_end = ext_beg + exts->extents[e].length - 1;

			/* check if the bad block overlaps with file's extent */
			if (bb_beg > ext_end || ext_beg > bb_end)
				continue;

			bb_found++;

			bb_beg = (bb_beg > ext_beg) ? bb_beg : ext_beg;
			bb_end = (bb_end < ext_end) ? bb_end : ext_end;
			bb_len = bb_end - bb_beg + 1;
			bb_off = bb_beg + exts->extents[e].offset_logical
					- exts->extents[e].offset_physical;

			/* check if offset is block-aligned */
			not_block_aligned = bb_off & (exts->blksize - 1);
			if (not_block_aligned) {
				bb_off -= not_block_aligned;
				bb_len += not_block_aligned;
			}

			/* check if length is block-aligned */
			bb_len = ALIGN_UP(bb_len, exts->blksize);

			LOG(4, "bad block found: offset: %llu, length: %llu",
				bb_off, bb_len);

			/* form a new bad block */
			struct bad_block bb;
			bb.offset = bb_off;
			bb.length = (unsigned)(bb_len);

			/* add the new bad block to the vector */
			if (VEC_PUSH_BACK(&bbv, bb)) {
				VEC_DELETE(&bbv);
				bb_found = -1;
				goto error_free_all;
			}
		}
	}

error_free_all:
	Free(bbs->bbv);
	bbs->bbv = NULL;
	bbs->bb_cnt = 0;

exit_free_all:
	if (exts) {
		Free(exts->extents);
		Free(exts);
	}

	if (extents > 0 && bb_found > 0) {
		bbs->bbv = VEC_ARR(&bbv);
		bbs->bb_cnt = (unsigned)VEC_SIZE(&bbv);

		LOG(10, "number of bad blocks detected: %u", bbs->bb_cnt);

		/* sanity check */
		ASSERTeq((unsigned)bb_found, bbs->bb_cnt);
	}

	return (bb_found >= 0) ? 0 : -1;
}

/*
 * os_badblocks_count -- returns number of bad blocks in the file
 *                       or -1 in case of an error
 */
long
os_badblocks_count(const char *file)
{
	LOG(3, "file %s", file);

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	int ret = os_badblocks_get(file, bbs);

	long count = (ret == 0) ? (long)bbs->bb_cnt : -1;

	badblocks_delete(bbs);

	return count;
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
		LOG(1, "counting bad blocks failed -- '%s'", file);
		return -1;
	}

	if (bbsc > 0) {
		LOG(1, "pool file '%s' contains %li bad block(s)", file, bbsc);
		return 1;
	}

	return 0;
}

/*
 * os_badblocks_clear_file -- clear the given bad blocks in the regular file
 *                            (not in a dax device)
 */
static int
os_badblocks_clear_file(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	int ret = 0;
	int fd;

	if ((fd = open(file, O_RDWR)) < 0) {
		ERR("!open: %s", file);
		return -1;
	}

	for (unsigned b = 0; b < bbs->bb_cnt; b++) {
		off_t offset = (off_t)bbs->bbv[b].offset;
		off_t length = (off_t)bbs->bbv[b].length;

		LOG(10,
			"clearing bad block: logical offset %li length %li (in 512B sectors)",
			B2SEC(offset), B2SEC(length));

		/* deallocate bad blocks */
		if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
				offset, length)) {
			ERR("!fallocate");
			ret = -1;
			break;
		}

		/* allocate new blocks */
		if (fallocate(fd, FALLOC_FL_KEEP_SIZE, offset, length)) {
			ERR("!fallocate");
			ret = -1;
			break;
		}
	}

	close(fd);

	return ret;
}

/*
 * os_badblocks_clear -- clears the given bad blocks in a file
 *                       (regular file or dax device)
 */
int
os_badblocks_clear(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	int file_type = util_file_get_type(file);
	if (file_type < 0)
		return -1;

	if (file_type == FILE_TYPE_DEVDAX)
		return os_dimm_devdax_clear_badblocks(file, bbs);

	return os_badblocks_clear_file(file, bbs);
}

/*
 * os_badblocks_clear_all -- clears all bad blocks in a file
 *                           (regular file or dax device)
 */
int
os_badblocks_clear_all(const char *file)
{
	LOG(3, "file %s", file);

	int file_type = util_file_get_type(file);
	if (file_type < 0)
		return -1;

	if (file_type == FILE_TYPE_DEVDAX)
		return os_dimm_devdax_clear_badblocks_all(file);

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	int ret = os_badblocks_get(file, bbs);
	if (ret) {
		LOG(1, "checking bad blocks in the file failed -- '%s'", file);
		goto error_free_all;
	}

	if (bbs->bb_cnt > 0) {
		ret = os_badblocks_clear_file(file, bbs);
		if (ret < 0) {
			LOG(1, "clearing bad blocks in the file failed -- '%s'",
				file);
			goto error_free_all;
		}
	}

error_free_all:
	badblocks_delete(bbs);

	return ret;
}
