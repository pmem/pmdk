// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * bad_blocks.c - implementation of the bad block API using libpmem2 library
 */

#include <fcntl.h>
#include <errno.h>

#include "libpmem2.h"
#include "badblocks.h"
#include "out.h"
#include "vec.h"
#include "os.h"

/*
 * badblocks_count -- returns number of bad blocks in the file
 *                    or -1 in case of an error
 */
long
badblocks_count(const char *file)
{
	LOG(3, "file %s", file);

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	int ret = badblocks_get(file, bbs);

	long count = (ret == 0) ? (long)bbs->bb_cnt : -1;

	badblocks_delete(bbs);

	return count;
}

/*
 * badblocks_get -- returns 0 and bad blocks in the 'bbs' array
 *                  (that has to be pre-allocated)
 *                  or -1 in case of an error
 */
int
badblocks_get(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	struct pmem2_source *src;
	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int bb_found = -1; /* -1 means an error */
	int ret;

	VEC(bbsvec, struct bad_block) bbv = VEC_INITIALIZER;
	memset(bbs, 0, sizeof(*bbs));

	int fd = os_open(file, O_RDONLY);
	if (fd == -1) {
		ERR("!open %s", file);
		return -1;
	}

	ret = pmem2_source_from_fd(&src, fd);
	if (ret)
		goto exit_close;

	ret = pmem2_badblock_context_new(&bbctx, src);
	if (ret)
		goto exit_delete_source;

	bb_found = 0;
	while ((pmem2_badblock_next(bbctx, &bb)) == 0) {
		bb_found++;
		/*
		 * Form a new bad block structure with offset and length
		 * expressed in bytes and offset relative
		 * to the beginning of the file.
		 */
		struct bad_block bbn;
		bbn.offset = bb.offset;
		bbn.length = bb.length;
		/* unknown healthy replica */
		bbn.nhealthy = NO_HEALTHY_REPLICA;

		/* add the new bad block to the vector */
		if (VEC_PUSH_BACK(&bbv, bbn)) {
			VEC_DELETE(&bbv);
			bb_found = -1;
			Free(bbs->bbv);
			bbs->bbv = NULL;
			bbs->bb_cnt = 0;
		}
	}

	if (bb_found > 0) {
		bbs->bbv = VEC_ARR(&bbv);
		bbs->bb_cnt = (unsigned)VEC_SIZE(&bbv);

		LOG(10, "number of bad blocks detected: %u", bbs->bb_cnt);

		/* sanity check */
		ASSERTeq((unsigned)bb_found, bbs->bb_cnt);
	}

	pmem2_badblock_context_delete(&bbctx);

exit_delete_source:
	pmem2_source_delete(&src);

exit_close:
	if (fd != -1)
		os_close(fd);

	if (ret && bb_found == -1)
		errno = pmem2_err_to_errno(ret);

	return (bb_found >= 0) ? 0 : -1;
}

/*
 * badblocks_clear -- clears the given bad blocks in a file
 *                    (regular file or dax device)
 */
int
badblocks_clear(const char *file, struct badblocks *bbs)
{
	LOG(3, "file %s badblocks %p", file, bbs);

	ASSERTne(bbs, NULL);

	struct pmem2_source *src;
	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int ret = -1;

	int fd = os_open(file, O_RDWR);
	if (fd == -1) {
		ERR("!open %s", file);
		return -1;
	}

	ret = pmem2_source_from_fd(&src, fd);
	if (ret)
		goto exit_close;

	ret = pmem2_badblock_context_new(&bbctx, src);
	if (ret) {
		LOG(1, "pmem2_badblock_context_new failed -- %s", file);
		goto exit_delete_source;
	}

	for (unsigned b = 0; b < bbs->bb_cnt; b++) {
		bb.offset = bbs->bbv[b].offset;
		bb.length = bbs->bbv[b].length;
		ret = pmem2_badblock_clear(bbctx, &bb);
		if (ret) {
			LOG(1, "pmem2_badblock_clear -- %s", file);
			goto exit_delete_ctx;
		}
	}

exit_delete_ctx:
	pmem2_badblock_context_delete(&bbctx);

exit_delete_source:
	pmem2_source_delete(&src);

exit_close:
	if (fd != -1)
		os_close(fd);

	if (ret) {
		errno = pmem2_err_to_errno(ret);
		ret = -1;
	}

	return ret;
}

/*
 * badblocks_clear_all -- clears all bad blocks in a file
 *                        (regular file or dax device)
 */
int
badblocks_clear_all(const char *file)
{
	LOG(3, "file %s", file);

	struct pmem2_source *src;
	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int ret = -1;

	int fd = os_open(file, O_RDWR);
	if (fd == -1) {
		ERR("!open %s", file);
		return -1;
	}

	ret = pmem2_source_from_fd(&src, fd);
	if (ret)
		goto exit_close;

	ret = pmem2_badblock_context_new(&bbctx, src);
	if (ret) {
		LOG(1, "pmem2_badblock_context_new failed -- %s", file);
		goto exit_delete_source;
	}

	while ((pmem2_badblock_next(bbctx, &bb)) == 0) {
		ret = pmem2_badblock_clear(bbctx, &bb);
		if (ret) {
			LOG(1, "pmem2_badblock_clear -- %s", file);
			goto exit_delete_ctx;
		}
	};

exit_delete_ctx:
	pmem2_badblock_context_delete(&bbctx);

exit_delete_source:
	pmem2_source_delete(&src);

exit_close:
	if (fd != -1)
		os_close(fd);

	if (ret) {
		errno = pmem2_err_to_errno(ret);
		ret = -1;
	}

	return ret;
}

/*
 * badblocks_check_file -- check if the file contains bad blocks
 *
 * Return value:
 * -1 : an error
 *  0 : no bad blocks
 *  1 : bad blocks detected
 */
int
badblocks_check_file(const char *file)
{
	LOG(3, "file %s", file);

	long bbsc = badblocks_count(file);
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
