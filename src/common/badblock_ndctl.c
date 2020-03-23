// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2018-2020, Intel Corporation */

/*
 * badblock_ndctl.c - implementation of the bad block API using ndctl library
 */

#define _GNU_SOURCE
#include <fcntl.h>
#include <linux/falloc.h>

#include "file.h"
#include "os.h"
#include "out.h"
#include "os_badblock.h"
#include "badblock.h"
#include "vec.h"
#include "../libpmem2/badblocks.h"

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

	if ((fd = os_open(file, O_RDWR)) < 0) {
		ERR("!open: %s", file);
		return -1;
	}

	for (unsigned b = 0; b < bbs->bb_cnt; b++) {
		off_t offset = (off_t)bbs->bbv[b].offset;
		off_t length = (off_t)bbs->bbv[b].length;

		LOG(10,
			"clearing bad block: logical offset %li length %li (in 512B sectors) -- '%s'",
			B2SEC(offset), B2SEC(length), file);

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

	os_close(fd);

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

	enum file_type type = util_file_get_type(file);
	if (type < 0)
		return -1;

	if (type == TYPE_DEVDAX)
		return badblocks_devdax_clear_badblocks(file, bbs);

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

	enum file_type type = util_file_get_type(file);
	if (type < 0)
		return -1;

	if (type == TYPE_DEVDAX)
		return badblocks_devdax_clear_badblocks_all(file);

	struct badblocks *bbs = badblocks_new();
	if (bbs == NULL)
		return -1;

	int ret = badblocks_get(file, bbs);
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
