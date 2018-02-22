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
 * os_badblock_linux.c - implementation of the linux bad block API
 */

#define _GNU_SOURCE
#include <fcntl.h>

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

	unsigned long long bb_beg, bb_end;
	unsigned long long off_beg, off_end;
	struct cb_data_s cbd;
	struct extents *exts;

	ASSERTne(bbs, NULL);

	cbd.bb_found = -1;

	exts = Zalloc(sizeof(struct extents));
	if (exts == NULL)
		return -1;

	if (os_dimm_files_namespace_badblocks(file, bbs))
		goto error_free_all;

	if (bbs->bbc == 0) {
		cbd.bb_found = 0;
		goto exit_free_all;
	}

	if (os_extents_get(file, exts))
		goto error_free_all;

	if (exts->extents_count == 0) {
		cbd.bb_found = (int)bbs->bbc;
		goto exit_free_all;
	}

	cbd.bb_found = 0;

	unsigned b, e;
	for (b = 0; b < bbs->bbc; b++) {
		for (e = 0; e < exts->extents_count; e++) {
			bb_beg = bbs->bbv[b].offset;
			bb_end = bb_beg + bbs->bbv[b].length - 1;

			off_beg = exts->extents[e].offset_physical;
			off_end = off_beg + exts->extents[e].length - 1;

			/* check if the bad block overlaps with file's extent */
			if (bb_beg > off_end || off_beg > bb_end)
				continue;

			cbd.bb_found++;

			if (cb) {
				cbd.beg = (bb_beg > off_beg) ? bb_beg : off_beg;
				cbd.end = (bb_end < off_end) ? bb_end : off_end;

				cbd.len = cbd.end - cbd.beg + 1;
				cbd.off = cbd.beg
					+ exts->extents[e].offset_logical
					- exts->extents[e].offset_physical;

				cbd.blksize = exts->blksize;

				if ((*cb)(&cbd, data))
					goto error_free_all;
			}
		}
	}

error_free_all:
	if (bbs->bbv) {
		Free(bbs->bbv);
		bbs->bbv = NULL;
	}
	bbs->bbc = 0;

exit_free_all:
	if (exts)
		Free(exts);

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
	if (bbs == NULL)
		return -1;

	long bb_found = os_badblocks_common(file, bbs, NULL, NULL);

	if (bbs->bbv)
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
		return -1;
	}

	if (bbsc > 0) {
		LOG(4, "pool file '%s' contains %li bad block(s)", file, bbsc);
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
	struct onebadblock **bbvp = arg;
	struct onebadblock *bbv = *bbvp;

	ASSERT(p->bb_found > 0);

	struct onebadblock *newbbv = Realloc(bbv,
			(size_t)(p->bb_found) * sizeof(struct onebadblock));
	if (newbbv == NULL) {
		if (bbv)
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

	if (bbs == NULL)
		return -1;

	struct onebadblock *bbvp = NULL;
	int bb_found = os_badblocks_common(file, bbs,
						os_badblocks_get_cb, &bbvp);
	if (bb_found < 0)
		goto error_free_all;

	if (bb_found) {
		LOG(10, "bad blocks detected: %u", bb_found);
	}

	bbs->bbc = (unsigned)bb_found;
	if (bbvp != NULL) {
		if (bbs->bbv)
			Free(bbs->bbv);
		bbs->bbv = bbvp;
	}

	return 0;

error_free_all:
	bbs->bbc = 0;
	if (bbs->bbv) {
		Free(bbs->bbv);
		bbs->bbv = NULL;
	}

	return -1;
}

/*
 * os_badblocks_clear_file_cb -- callback for os_badblocks_clear_file
 */
static int
os_badblocks_clear_file_cb(struct cb_data_s *p, void *arg)
{
	unsigned long long beg, off, len, not_block_aligned;
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
		"clearing bad block: physical offset %llu logical offset %llu length %llu (sectors)",
		beg >> 9, off >> 9, len >> 9);

	/* deallocate bad blocks */
	if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
						(off_t)off, (off_t)len)) {
		perror("fallocate");
	}

	/* allocate new blocks */
	if (fallocate(fd, FALLOC_FL_KEEP_SIZE, (off_t)off, (off_t)len)) {
		perror("fallocate");
	}

	return 0;
}

/*
 * os_badblocks_clear_file -- clears bad blocks in the regular file
 *                            (not in a dax device)
 */
static int
os_badblocks_clear_file(const char *file)
{
	LOG(3, "file %s", file);

	struct badblocks *bbs;
	int bb_found = -1;
	int fd = -1;

	if ((fd = os_open(file, O_RDWR)) < 0)
		return -1;

	bbs = Zalloc(sizeof(struct badblocks));
	if (bbs == NULL)
		goto exit_close;

	bb_found = os_badblocks_common(file, bbs,
					os_badblocks_clear_file_cb, &fd);
	if (bbs->bbv)
		Free(bbs->bbv);
	Free(bbs);

exit_close:
	close(fd);

	return bb_found;
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
