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

/* callback's data structure */
struct cb_data_s {
	int bb_found;
	unsigned long long beg;
	unsigned long long end;
	unsigned long long off;
	unsigned long long len;
	unsigned long long blksize;
};

/*
 * os_badblocks_common -- returns number of bad blocks in the file
 *                        or -1 in case of an error
 */
static int
os_badblocks_common(const char *file, struct badblocks *bbs,
			int (*cb)(struct cb_data_s *, void *),
			void *data)
{
	LOG(3, "file %s", file);

	unsigned long long bb_beg;
	unsigned long long bb_end;
	unsigned long long off_beg;
	unsigned long long off_end;
	struct cb_data_s cbd;
	struct extents *exts = NULL;

	ASSERTne(bbs, NULL);

	cbd.bb_found = -1;

	if (os_dimm_files_namespace_badblocks(file, bbs)) {
		ERR("checking the file for bad blocks failed -- '%s'", file);
		goto error_free_all;
	}

	if (bbs->bb_cnt == 0)
		return 0;

	exts = Zalloc(sizeof(struct extents));
	if (exts == NULL) {
		ERR("!malloc");
		goto error_free_all;
	}

	long count = os_extents_count(file, exts);
	if (count < 0) {
		ERR("counting file's extents failed -- '%s'", file);
		goto error_free_all;
	}

	if (count == 0) {
		cbd.bb_found = (int)bbs->bb_cnt;
		goto exit_free_all;
	}

	exts->extents = Zalloc(exts->extents_count * sizeof(struct extent));
	if (exts->extents == NULL) {
		ERR("!malloc");
		goto error_free_all;
	}

	if (os_extents_get(file, exts)) {
		ERR("getting file's extents failed -- '%s'", file);
		goto error_free_all;
	}

	cbd.bb_found = 0;

	unsigned b, e;
	for (b = 0; b < bbs->bb_cnt; b++) {
		for (e = 0; e < exts->extents_count; e++) {
			bb_beg = bbs->bbv[b].offset;
			bb_end = bb_beg + bbs->bbv[b].length - 1;

			off_beg = exts->extents[e].offset_physical;
			off_end = off_beg + exts->extents[e].length - 1;

			/* check if the bad block overlaps with file's extent */
			if (bb_beg > off_end || off_beg > bb_end)
				continue;

			cbd.bb_found++;

			cbd.beg = (bb_beg > off_beg) ? bb_beg : off_beg;
			cbd.end = (bb_end < off_end) ? bb_end : off_end;

			cbd.len = cbd.end - cbd.beg + 1;
			cbd.off = cbd.beg + exts->extents[e].offset_logical
					- exts->extents[e].offset_physical;

			cbd.blksize = exts->blksize;

			LOG(4, "bad block found: offset: %llu, length: %llu",
				cbd.off, cbd.len);

			if (cb && (*cb)(&cbd, data))
				goto error_free_all;
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

	return cbd.bb_found;
}

/*
 * os_badblocks_count -- returns number of bad blocks in the file
 *                       or -1 in case of an error
 */
long
os_badblocks_count(const char *file)
{
	LOG(3, "file %s", file);

	struct badblocks *bbs = Zalloc(sizeof(struct badblocks));
	if (bbs == NULL) {
		ERR("!malloc");
		return -1;
	}

	long bb_found = os_badblocks_common(file, bbs, NULL, NULL);

	Free(bbs->bbv);
	Free(bbs);

	return bb_found;
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
		ERR("counting bad blocks failed -- '%s'", file);
		return -1;
	}

	if (bbsc > 0) {
		LOG(1, "pool file '%s' contains %li bad block(s)", file, bbsc);
		return 1;
	}

	return 0;
}

/*
 * os_badblocks_get_cb -- callback for os_badblocks_get
 */
static int
os_badblocks_get_cb(struct cb_data_s *p, void *arg)
{
	LOG(3, "cb_data_s %p arg %p", p, arg);

	struct bad_block **bbvp = arg;
	struct bad_block *bbv = *bbvp;

	ASSERT(p->bb_found > 0);

	struct bad_block *newbbv = Realloc(bbv,
			(size_t)(p->bb_found) * sizeof(struct bad_block));
	if (newbbv == NULL) {
		ERR("!realloc");
		Free(bbv);
		return -1;
	}

	bbv = newbbv;
	bbv[p->bb_found - 1].length = (unsigned)(p->len);
	bbv[p->bb_found - 1].offset = p->off;

	*bbvp = bbv;

	return 0;
}

/*
 * os_badblocks_get -- returns list of bad blocks in the file
 */
int
os_badblocks_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s", file);

	ASSERTne(bbs, NULL);

	struct bad_block *bbvp = NULL;
	int bb_found = os_badblocks_common(file, bbs,
						os_badblocks_get_cb, &bbvp);
	if (bb_found < 0)
		goto error_free_all;

	if (bb_found) {
		LOG(10, "bad blocks detected: %u", bb_found);
	}

	bbs->bb_cnt = (unsigned)bb_found;
	if (bbvp != NULL) {
		Free(bbs->bbv);
		bbs->bbv = bbvp;
	}

	return 0;

error_free_all:
	bbs->bb_cnt = 0;
	Free(bbs->bbv);
	bbs->bbv = NULL;

	return -1;
}

/*
 * os_badblocks_clear_file_cb -- callback for os_badblocks_clear_file
 */
static int
os_badblocks_clear_file_cb(struct cb_data_s *p, void *arg)
{
	LOG(3, "cb_data_s %p arg %p", p, arg);

	unsigned long long beg;
	unsigned long long off;
	unsigned long long len;
	unsigned long long not_block_aligned;
	int fd = *(int *)arg;

	/* check if off is block-aligned */
	not_block_aligned = p->off & (p->blksize - 1);
	if (not_block_aligned) {
		beg = p->beg - not_block_aligned;
		off = p->off - not_block_aligned;
		len = p->len + not_block_aligned;
	} else {
		beg = p->beg;
		off = p->off;
		len = p->len;
	}

	/* check if len is block-aligned */
	not_block_aligned = len & (p->blksize - 1);
	if (not_block_aligned) {
		len += p->blksize - not_block_aligned;
	}

	LOG(10,
		"clearing bad block: physical offset %llu logical offset %llu length %llu (in 512B sectors)",
		B2SEC(beg), B2SEC(off), B2SEC(len));

	/* deallocate bad blocks */
	if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
						(off_t)off, (off_t)len)) {
		ERR("!fallocate");

	}

	/* allocate new blocks */
	if (fallocate(fd, FALLOC_FL_KEEP_SIZE, (off_t)off, (off_t)len)) {
		ERR("!fallocate");
	}

	return 0;
}

/*
 * os_badblocks_get_clear_file -- get and clear bad blocks in the regular file
 *                                (not in a dax device)
 */
static int
os_badblocks_get_clear_file(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	int allocated_bbs = 0;
	int bb_found = -1;
	int fd = -1;

	if ((fd = open(file, O_RDWR)) < 0) {
		ERR("!open: %s", file);
		return -1;
	}

	if (bbs) {
		memset(bbs, 0, sizeof(*bbs));
	} else {
		bbs = Zalloc(sizeof(struct badblocks));
		if (bbs == NULL) {
			ERR("!malloc");
			goto exit_close;
		}
		allocated_bbs = 1;
	}

	bb_found = os_badblocks_common(file, bbs,
					os_badblocks_clear_file_cb, &fd);

	if (allocated_bbs) {
		Free(bbs->bbv);
		Free(bbs);
	}

exit_close:
	close(fd);

	return bb_found;
}

/*
 * os_badblocks_get_and_clear -- get and clear bad blocks in a file
 *                               (regular file or dax device)
 */
int
os_badblocks_get_and_clear(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	if (util_file_is_device_dax(file))
		return os_dimm_devdax_clear_get_badblocks(file, bbs);

	return os_badblocks_get_clear_file(file, bbs);
}

/*
 * os_badblocks_clear -- clears bad blocks in a file
 *                      (regular file or dax device)
 */
int
os_badblocks_clear(const char *file)
{
	LOG(3, "file %s", file);

	return os_badblocks_get_and_clear(file, NULL);
}
