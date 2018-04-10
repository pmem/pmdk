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

/* bad blocks operation modes */
enum op_mode {
	OP_COUNT = 0,	/* count bad blocks */
	OP_GET = 1,	/* get bad blocks */
	OP_CLEAR = 2,	/* clear bad blocks */
};

/* operation's bad block structure */
struct op_bb_s {
	int bb_found;
	unsigned long long beg;
	unsigned long long end;
	unsigned long long off;
	unsigned long long len;
	unsigned long long blksize;
};

/*
 * os_badblocks_clear_file_op -- 'clear badblocks in a file' operation
 */
static int
os_badblocks_clear_file_op(struct op_bb_s *bs, int fd)
{
	LOG(3, "op_bb_s %p fd %i", bs, fd);

	unsigned long long not_block_aligned;

	/* check if off is block-aligned */
	not_block_aligned = bs->off & (bs->blksize - 1);
	if (not_block_aligned) {
		bs->beg -= not_block_aligned;
		bs->off -= not_block_aligned;
		bs->len += not_block_aligned;
	}

	/* check if len is block-aligned */
	not_block_aligned = bs->len & (bs->blksize - 1);
	if (not_block_aligned) {
		bs->len += bs->blksize - not_block_aligned;
	}

	LOG(10,
		"clearing bad block: physical offset %llu logical offset %llu length %llu (in 512B sectors)",
		B2SEC(bs->beg), B2SEC(bs->off), B2SEC(bs->len));

	/* deallocate bad blocks */
	if (fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
					(off_t)bs->off, (off_t)bs->len)) {
		ERR("!fallocate");
		return -1;
	}

	/* allocate new blocks */
	if (fallocate(fd, FALLOC_FL_KEEP_SIZE,
					(off_t)bs->off, (off_t)bs->len)) {
		ERR("!fallocate");
		return -1;
	}

	return 0;
}

/*
 * os_badblocks_get_op -- 'get badblocks' operation
 */
static int
os_badblocks_get_op(struct op_bb_s *bs, struct bad_block **bbvp)
{
	LOG(3, "op_bb_s %p bbvp %p", bs, bbvp);

	ASSERTne(bbvp, NULL);
	ASSERT(bs->bb_found > 0);

	struct bad_block *bbv = *bbvp;

	struct bad_block *newbbv = Realloc(bbv,
			(size_t)(bs->bb_found) * sizeof(struct bad_block));
	if (newbbv == NULL) {
		ERR("!realloc");
		Free(bbv);
		return -1;
	}

	bbv = newbbv;
	bbv[bs->bb_found - 1].length = (unsigned)(bs->len);
	bbv[bs->bb_found - 1].offset = bs->off;

	LOG(10, "getting bad block: offset %llu length %llu", bs->off, bs->len);

	*bbvp = bbv;

	return 0;
}

/*
 * os_badblocks_common -- returns number of bad blocks in the file
 *                        or -1 in case of an error
 */
static int
os_badblocks_common(enum op_mode op, const char *file, struct badblocks *bbs,
			int fd)
{
	LOG(3, "op 0x%x file %s badblocks %p fd %i", op, file, bbs, fd);

	unsigned long long bb_beg;
	unsigned long long bb_end;
	unsigned long long off_beg;
	unsigned long long off_end;
	struct extents *exts = NULL;
	struct bad_block *bbv = NULL;
	struct op_bb_s bs;

	ASSERTne(bbs, NULL);

	bs.bb_found = -1;

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
		bs.bb_found = (int)bbs->bb_cnt;
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

	bs.bb_found = 0;

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

			bs.bb_found++;

			bs.beg = (bb_beg > off_beg) ? bb_beg : off_beg;
			bs.end = (bb_end < off_end) ? bb_end : off_end;

			bs.len = bs.end - bs.beg + 1;
			bs.off = bs.beg + exts->extents[e].offset_logical
					- exts->extents[e].offset_physical;

			bs.blksize = exts->blksize;

			LOG(4, "bad block found: offset: %llu, length: %llu",
				bs.off, bs.len);

			if (op & OP_CLEAR)
				if (os_badblocks_clear_file_op(&bs, fd))
					goto error_free_all;

			if (op & OP_GET)
				if (os_badblocks_get_op(&bs, &bbv))
					goto error_free_all;
		}
	}

	Free(bbs->bbv);

	if (op & OP_GET) {
		bbs->bb_cnt = (unsigned)bs.bb_found;
		bbs->bbv = bbv;
	} else {
		bbs->bb_cnt = 0;
		bbs->bbv = NULL;
	}

exit_free_all:
	if (exts) {
		Free(exts->extents);
		Free(exts);
	}

	return bs.bb_found;

error_free_all:
	Free(bbv);
	Free(bbs->bbv);
	bbs->bbv = NULL;
	bbs->bb_cnt = 0;

	if (exts) {
		Free(exts->extents);
		Free(exts);
	}

	return -1;
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

	long bb_found = os_badblocks_common(OP_COUNT, file, bbs, 0);

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
 * os_badblocks_get -- returns list of bad blocks in the file
 */
int
os_badblocks_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s", file);

	ASSERTne(bbs, NULL);

	int bb_found = os_badblocks_common(OP_GET, file, bbs, 0);
	if (bb_found < 0)
		goto error_free_all;

	if (bb_found) {
		LOG(10, "bad blocks detected: %u", bb_found);
	}

	return 0;

error_free_all:
	bbs->bb_cnt = 0;
	Free(bbs->bbv);
	bbs->bbv = NULL;

	return -1;
}

/*
 * os_badblocks_clear_get_file -- clear and get bad blocks in the regular file
 *                                (not in a dax device)
 */
static int
os_badblocks_clear_get_file(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	int bb_found = -1;
	int fd;

	if ((fd = open(file, O_RDWR)) < 0) {
		ERR("!open: %s", file);
		return -1;
	}

	if (bbs) { /* clear and get */
		memset(bbs, 0, sizeof(*bbs));
		bb_found = os_badblocks_common(OP_CLEAR | OP_GET,
						file, bbs, fd);

	} else { /* clear only */
		bbs = Zalloc(sizeof(struct badblocks));
		if (bbs == NULL) {
			ERR("!malloc");
			goto exit_close;
		}

		bb_found = os_badblocks_common(OP_CLEAR, file, bbs, fd);

		Free(bbs->bbv);
		Free(bbs);
	}

exit_close:
	close(fd);

	return bb_found;
}

/*
 * os_badblocks_clear_and_get -- clear and get bad blocks in a file
 *                               (regular file or dax device)
 */
int
os_badblocks_clear_and_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	if (util_file_is_device_dax(file))
		return os_dimm_devdax_clear_get_badblocks(file, bbs);

	return os_badblocks_clear_get_file(file, bbs);
}

/*
 * os_badblocks_clear -- clears bad blocks in a file
 *                      (regular file or dax device)
 */
int
os_badblocks_clear(const char *file)
{
	LOG(3, "file %s", file);

	return os_badblocks_clear_and_get(file, NULL);
}
