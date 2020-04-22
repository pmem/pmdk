// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_badblock_mocks.c -- unit test for pmem2_badblock_next()
 */

#include "unittest.h"
#include "out.h"
#include "source.h"
#include "badblocks.h"
#include "pmem2_badblock_mocks.h"

#define MAX_BAD_BLOCKS_NUMBER 10
#define MAX_EXTENTS_NUMBER 8

typedef struct badblock bad_blocks_array[MAX_BAD_BLOCKS_NUMBER];

/* HW bad blocks expressed in 512b sectors */
static bad_blocks_array hw_bad_blocks[] =
{
	/* fd % 50 == 0 - test_basic - did not found any matching device */
	{ {0, 0} },
	/* fd % 50 == 1 - test_basic - no bad blocks */
	{ {0, 0} },
	/* fd % 50 == 2 - test_read_bad_blocks #1 */
	{ {1, 1}, {0, 0} },
	/* fd % 50 == 3 - test_read_bad_blocks #2 */
	{ {4, 10}, {16, 10}, {28, 2}, {32, 4}, {40, 4}, {50, 2}, {0, 0} },
	/* fd % 50 == 4 - test_read_bad_blocks #3 */
	{ {2, 4}, {8, 2}, {12, 6}, {20, 2}, {24, 10}, {38, 4}, {46, 2}, \
	{0, 0} },
};

/* file's bad blocks expressed in 512b sectors */
static bad_blocks_array file_bad_blocks[] =
{
	/* fd % 50 == 0 - test_basic - did not found any matching device */
	{ {0, 0} },
	/* fd % 50 == 1 - test_basic - no bad blocks */
	{ {0, 0} },
	/* fd % 50 == 2 - test_read_bad_blocks #1 */
	{ {0, 2}, {0, 0} },
	/* fd % 50 == 3 - test_read_bad_blocks #2 */
	{ {4, 2}, {8, 2}, {12, 2}, {16, 2}, {20, 2}, {24, 2}, {28, 2}, \
	{32, 2}, {40, 2}, {0, 0} },
	/* fd % 50 == 4 - test_read_bad_blocks #3 */
	{ {4, 2}, {8, 2}, {12, 2}, {16, 2}, {20, 2}, {24, 2}, {28, 2}, \
	{32, 2}, {40, 2}, {0, 0} },
};

/* file's extents expressed in 512b sectors */
static struct extent files_extents[][MAX_EXTENTS_NUMBER] =
{
	/* fd % 50 == 0 - test_basic - did not found any matching device */
	{ {0, 0, 0} },
	/* fd % 50 == 1 - test_basic - no bad blocks */
	{ {0, 0, 0} },
	/* fd % 50 == 2 - test_read_bad_blocks #1 */
	{ {0, 0, 2}, {0, 0, 0} },
	/* fd % 50 == 3 - test_read_bad_blocks #2 */
	{ {2, 2, 4}, {8, 8, 2}, {12, 12, 6}, {20, 20, 2}, {24, 24, 10}, \
	{38, 38, 4}, {46, 46, 2}, {0, 0, 0} },
	/* fd % 50 == 4 - test_read_bad_blocks #3 */
	{ {4, 4, 10}, {16, 16, 10}, {28, 28, 2}, {32, 32, 4}, {40, 40, 4}, \
	{50, 50, 2}, {0, 0, 0} },
};

/*
 * map_test_to_set -- map number of test to number of set of bad blocks
 */
static unsigned
map_test_to_set(unsigned test)
{
	unsigned set = test % MASK_SET;
	return set;
}

/*
 * get_next_typed_badblock -- get next typed badblock
 */
static struct badblock *
get_next_typed_badblock(unsigned test, unsigned *i_bb,
		bad_blocks_array bad_blocks[])
{
	unsigned set = map_test_to_set(test);
	struct badblock *bb = &bad_blocks[set][*i_bb];
	if (bb->offset == 0 && bb->len == 0)
		bb = NULL; /* no more bad blocks */
	else
		(*i_bb)++;

	return bb;
}

/*
 * get_next_hw_badblock -- get next HW badblock
 */
struct badblock *
get_next_hw_badblock(unsigned test, unsigned *i_bb)
{
	return get_next_typed_badblock(test, i_bb, hw_bad_blocks);
}

/*
 * get_next_file_badblock -- get next file's badblock
 */
static struct badblock *
get_next_file_badblock(unsigned test, unsigned *i_bb)
{
	return get_next_typed_badblock(test, i_bb, file_bad_blocks);
}

/*
 * get_next_badblock -- get next badblock
 */
static struct badblock *
get_next_badblock(int fd, unsigned *i_bb)
{
	UT_ASSERT(fd > 0);

	if (fd < FD_CHR_DEV)
		/* regular file */
		return get_next_file_badblock((unsigned)fd, i_bb);

	/* DAX device */
	return get_next_hw_badblock((unsigned)fd, i_bb);
}

/*
 * get_extents -- get file's extents
 */
int
get_extents(int fd, struct extents **exts)
{
	UT_ASSERTinfo(fd < FD_CHR_DEV, "Not a regular file");

	unsigned set = map_test_to_set((unsigned)fd);

	*exts = ZALLOC(sizeof(struct extents));
	struct extents *pexts = *exts;

	/* set block size */
	pexts->blksize = BLK_SIZE_1KB;

	/* count extents (length > 0) */
	while (files_extents[set][pexts->extents_count].length)
		pexts->extents_count++;

	pexts->extents = MALLOC(pexts->extents_count * sizeof(struct extent));

	for (int i = 0; i < pexts->extents_count; i++) {
		struct extent ext = files_extents[set][i];
		uint64_t off_phy = ext.offset_physical;
		uint64_t off_log = ext.offset_logical;
		uint64_t len = ext.length;

		/* check alignment */
		UT_ASSERTeq(SEC2B(off_phy) % pexts->blksize, 0);
		UT_ASSERTeq(SEC2B(off_log) % pexts->blksize, 0);
		UT_ASSERTeq(SEC2B(len) % pexts->blksize, 0);

		pexts->extents[i].offset_physical = SEC2B(off_phy);
		pexts->extents[i].offset_logical = SEC2B(off_log);
		pexts->extents[i].length = SEC2B(len);
	}

	return 0;
}

/*
 * test_basic -- basic test
 */
static int
test_basic(struct pmem2_source *src)
{
	UT_OUT("TEST: test_basic: %i", src->fd);

	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int ret;

	ret = pmem2_badblock_context_new(src, &bbctx);
	if (ret)
		return ret;

	ret = pmem2_badblock_next(bbctx, &bb);
	if (ret)
		goto exit_free;

exit_free:
	pmem2_badblock_context_delete(&bbctx);

	return ret;
}

/*
 * test_read_bad_blocks -- test reading bad blocks
 */
static int
test_read_bad_blocks(struct pmem2_source *src)
{
	UT_OUT("TEST: test_read_bad_blocks: %i", src->fd);

	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	struct badblock *bb2;
	unsigned i_bb;
	int ret;

	ret = pmem2_badblock_context_new(src, &bbctx);
	if (ret)
		return ret;

	i_bb = 0;
	while ((ret = pmem2_badblock_next(bbctx, &bb)) == 0) {
		bb2 = get_next_badblock(src->fd, &i_bb);
		UT_ASSERTne(bb2, NULL);
		UT_ASSERTeq(bb.offset, SEC2B(bb2->offset));
		UT_ASSERTeq(bb.length, SEC2B(bb2->len));
	}

	bb2 = get_next_badblock(src->fd, &i_bb);
	UT_ASSERTeq(bb2, NULL);

	pmem2_badblock_context_delete(&bbctx);

	return ret;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_badblock_next");

	/*
	 *   0 <= fd <= 100 - regular file, did not found any matching device
	 * 101 <= fd <= 200 - regular file
	 * 201 <= fd <= 300 - character device
	 * 301 <= fd <= 400 - directory
	 * 401 <= fd        - block device
	 *
	 * fd % 100 == 0  - did not found any matching device
	 * fd % 100 <  50 - namespace mode
	 * fd % 100 >= 50 - region mode
	 */
	struct pmem2_source src;

	/* PART #1 - basic tests */

	/* did not found any matching device */
	src.fd = MODE_NO_DEV;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: did not found any matching device");

	/* regular file, namespace mode */
	src.fd = FD_REG_FILE + MODE_NAMESPACE;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: regular file, namespace mode");

	/* regular file, region mode */
	src.fd = FD_REG_FILE + MODE_REGION;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: regular file, region mode");

	/* character device, namespace mode */
	src.fd = FD_CHR_DEV + MODE_NAMESPACE;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, namespace mode");

	/* character device, region mode */
	src.fd = FD_CHR_DEV + MODE_REGION;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	/* directory */
	src.fd = FD_DIRECTORY;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: directory");

	/* block device */
	src.fd = FD_BLK_DEV;
	UT_ASSERTinfo(test_basic(&src) == PMEM2_E_INVALID_FILE_TYPE,
			"FAILED: block device");

	/* PART #2 - test reading bad blocks */

	/* test #1 */

	/* test #1: regular file, namespace mode */
	src.fd = TEST_BB_REG_NAMESPACE_1;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, namespace mode");

	/* test #1: regular file, region mode */
	src.fd = TEST_BB_REG_REGION_1;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	/* test #1: character device, region mode */
	src.fd = TEST_BB_CHR_REGION_1;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	/* test #2 */

	/* test #2: regular file, namespace mode */
	src.fd = TEST_BB_REG_NAMESPACE_2;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, namespace mode");

	/* test #2: regular file, region mode */
	src.fd = TEST_BB_REG_REGION_2;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	/* test #2: character device, region mode */
	src.fd = TEST_BB_CHR_REGION_2;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	/* test #3 */

	/* test #3: regular file, namespace mode */
	src.fd = TEST_BB_REG_NAMESPACE_3;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, namespace mode");

	/* test #3: regular file, region mode */
	src.fd = TEST_BB_REG_REGION_3;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	/* test #3: character device, region mode */
	src.fd = TEST_BB_CHR_REGION_3;
	UT_ASSERTinfo(test_read_bad_blocks(&src) == PMEM2_E_NO_BAD_BLOCK_FOUND,
			"FAILED: character device, region mode");

	DONE(NULL);
}
