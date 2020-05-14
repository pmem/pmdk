// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2020, Intel Corporation */

/*
 * pmem2_badblock_mocks.c -- unit test for pmem2_badblock_*()
 */

#include <ndctl/libndctl.h>

#include "unittest.h"
#include "out.h"
#include "source.h"
#include "badblocks.h"
#include "pmem2_badblock_mocks.h"

#define BAD_BLOCKS_NUMBER 10
#define EXTENTS_NUMBER 8
#define MAX_BB_SET_STR "4"
#define MAX_BB_SET 4
#define DEFAULT_BB_SET 1

#define USAGE_MSG \
"Usage: pmem2_badblock_mocks <test_case> <file_type> <mode> [bad_blocks_set]\n"\
"Possible values of arguments:\n"\
"   test_case      :     test_basic, test_read_clear_bb \n"\
"   file_type      :     reg_file, chr_dev, directory, blk_dev\n"\
"   mode           :     no_device, namespace, region\n"\
"   bad_blocks_set :     1-"MAX_BB_SET_STR"\n\n"

/* indexes of arguments */
enum args_t {
	ARG_TEST_CASE = 1,
	ARG_FILE_TYPE,
	ARG_MODE,
	ARG_BB_SET,
	/* it always has to be the last one */
	ARG_NUMBER, /* number of arguments */
};

typedef int test_fn(struct pmem2_source *src);
typedef struct badblock bad_blocks_array[BAD_BLOCKS_NUMBER];

/* HW bad blocks expressed in 512b sectors */
static bad_blocks_array hw_bad_blocks[] =
{
	/* test #1 - no bad blocks */
	{ {0, 0} },
	/* test #2 - 1 HW bad block */
	{ {1, 1}, {0, 0} },
	/* test #3 - 6 HW bad blocks */
	{ {4, 10}, {16, 10}, {28, 2}, {32, 4}, {40, 4}, {50, 2}, {0, 0} },
	/* test #4 - 7 HW bad blocks */
	{ {2, 4}, {8, 2}, {12, 6}, {20, 2}, {24, 10}, {38, 4}, {46, 2}, \
	    {0, 0} },
};

/* file's bad blocks expressed in 512b sectors */
static bad_blocks_array file_bad_blocks[] =
{
	/* test #1 - no bad blocks */
	{ {0, 0} },
	/* test #2 - 1 file bad block */
	{ {0, 2}, {0, 0} },
	/* test #3 - 9 file bad blocks */
	{ {4, 2}, {8, 2}, {12, 2}, {16, 2}, {20, 2}, {24, 2}, {28, 2}, \
	    {32, 2}, {40, 2}, {0, 0} },
	/* test #4 - 9 file bad blocks */
	{ {4, 2}, {8, 2}, {12, 2}, {16, 2}, {20, 2}, {24, 2}, {28, 2}, \
	    {32, 2}, {40, 2}, {0, 0} },
};

/* file's extents expressed in 512b sectors */
static struct extent files_extents[][EXTENTS_NUMBER] =
{
	/* test #1 - no extents */
	{ {0, 0, 0} },
	/* test #2 - 1 extent */
	{ {0, 0, 2}, {0, 0, 0} },
	/* test #3 - 7 extents */
	{ {2, 2, 4}, {8, 8, 2}, {12, 12, 6}, {20, 20, 2}, {24, 24, 10}, \
	    {38, 38, 4}, {46, 46, 2}, {0, 0, 0} },
	/* test #4 - 6 extents */
	{ {4, 4, 10}, {16, 16, 10}, {28, 28, 2}, {32, 32, 4}, {40, 40, 4}, \
	    {50, 50, 2}, {0, 0, 0} },
};

/*
 * map_test_to_set -- map number of a test to an index of bad blocks' set
 */
static inline unsigned
map_test_to_set(unsigned test)
{
	return test & MASK_TEST;
}

/*
 * get_nth_typed_badblock -- get next typed badblock
 */
static struct badblock *
get_nth_typed_badblock(unsigned test, unsigned *i_bb,
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
 * get_nth_hw_badblock -- get next HW badblock
 */
struct badblock *
get_nth_hw_badblock(unsigned test, unsigned *i_bb)
{
	return get_nth_typed_badblock(test, i_bb, hw_bad_blocks);
}

/*
 * get_nth_file_badblock -- get next file's badblock
 */
static struct badblock *
get_nth_file_badblock(unsigned test, unsigned *i_bb)
{
	return get_nth_typed_badblock(test, i_bb, file_bad_blocks);
}

/*
 * get_nth_badblock -- get next badblock
 */
static struct badblock *
get_nth_badblock(int fd, unsigned *i_bb)
{
	UT_ASSERT(fd >= 0);

	if ((fd & MASK_MODE) == MODE_NO_DEVICE)
		/* no matching device found */
		return NULL;

	switch (fd & MASK_DEVICE) {
	case FD_REG_FILE: /* regular file */
		return get_nth_file_badblock((unsigned)fd, i_bb);
	case FD_CHR_DEV: /* character device */
		return get_nth_hw_badblock((unsigned)fd, i_bb);
	case FD_DIRECTORY:
	case FD_BLK_DEV:
		break;
	}

	/* no bad blocks found */
	return NULL;
}

/*
 * get_extents -- get file's extents
 */
int
get_extents(int fd, struct extents **exts)
{
	unsigned set = map_test_to_set((unsigned)fd);

	*exts = ZALLOC(sizeof(struct extents));
	struct extents *pexts = *exts;

	/* set block size */
	pexts->blksize = BLK_SIZE_1KB;

	if ((fd & MASK_DEVICE) != FD_REG_FILE) {
		/* not a regular file */
		return 0;
	}

	/* count extents (length > 0) */
	while (files_extents[set][pexts->extents_count].length)
		pexts->extents_count++;

	/*
	 * It will be freed internally by libpmem2
	 * (pmem2_badblock_context_delete)
	 */
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
	UT_OUT("TEST: test_basic: 0x%x", src->fd);

	struct pmem2_badblock_context *bbctx;
	struct pmem2_badblock bb;
	int ret;

	ret = pmem2_badblock_context_new(src, &bbctx);
	if (ret)
		return ret;

	ret = pmem2_badblock_next(bbctx, &bb);
	pmem2_badblock_context_delete(&bbctx);

	return ret;
}

/*
 * test_read_clear_bb -- test reading and clearing bad blocks
 */
static int
test_read_clear_bb(struct pmem2_source *src)
{
	UT_OUT("TEST: test_read_clear_bb: 0x%x", src->fd);

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
		bb2 = get_nth_badblock(src->fd, &i_bb);
		UT_ASSERTne(bb2, NULL);
		UT_ASSERTeq(bb.offset, SEC2B(bb2->offset));
		UT_ASSERTeq(bb.length, SEC2B(bb2->len));
		ret = pmem2_badblock_clear(bbctx, &bb);
		if (ret)
			goto exit_free;
	}

	bb2 = get_nth_badblock(src->fd, &i_bb);
	UT_ASSERTeq(bb2, NULL);

exit_free:
	pmem2_badblock_context_delete(&bbctx);

	return ret;
}

static void
parse_arguments(int argc, char *argv[], int *test, test_fn **test_func)
{
	if (argc < (ARG_NUMBER - 1) || argc > ARG_NUMBER) {
		UT_OUT(USAGE_MSG);
		if (argc > ARG_NUMBER)
			UT_FATAL("too many arguments");
		else
			UT_FATAL("missing required argument(s)");
	}

	char *test_case = argv[ARG_TEST_CASE];
	char *file_type = argv[ARG_FILE_TYPE];
	char *mode = argv[ARG_MODE];

	*test = 0;
	*test_func = NULL;

	if (strcmp(test_case, "test_basic") == 0) {
		*test_func = test_basic;
	} else if (strcmp(test_case, "test_read_clear_bb") == 0) {
		*test_func = test_read_clear_bb;
	} else {
		UT_OUT(USAGE_MSG);
		UT_FATAL("wrong test case: %s", test_case);
	}

	if (strcmp(file_type, "reg_file") == 0) {
		*test |= FD_REG_FILE;
	} else if (strcmp(file_type, "chr_dev") == 0) {
		*test |= FD_CHR_DEV;
	} else if (strcmp(file_type, "directory") == 0) {
		*test |= FD_DIRECTORY;
	} else if (strcmp(file_type, "blk_dev") == 0) {
		*test |= FD_BLK_DEV;
	} else {
		UT_OUT(USAGE_MSG);
		UT_FATAL("wrong file type: %s", file_type);
	}

	if (strcmp(mode, "no_device") == 0) {
		*test |= MODE_NO_DEVICE;
	} else if (strcmp(mode, "namespace") == 0) {
		*test |= MODE_NAMESPACE;
	} else if (strcmp(mode, "region") == 0) {
		*test |= MODE_REGION;
	} else {
		UT_OUT(USAGE_MSG);
		UT_FATAL("wrong mode: %s", mode);
	}

	int bad_blocks_set =
		(argc == 5) ? atoi(argv[ARG_BB_SET]) : DEFAULT_BB_SET;
	if (bad_blocks_set >= 1 && bad_blocks_set <= MAX_BB_SET) {
		*test |= (bad_blocks_set - 1);
	} else {
		UT_OUT(USAGE_MSG);
		UT_FATAL("wrong bad_blocks_set: %i", bad_blocks_set);
	}
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_badblock_mocks");

	/* sanity check of defines */
	UT_ASSERTeq(atoi(MAX_BB_SET_STR), MAX_BB_SET);

	struct pmem2_source src;
	test_fn *test_func;

	parse_arguments(argc, argv, &src.fd, &test_func);

	int expected_result;
	if (src.fd < FD_DIRECTORY)
		expected_result = PMEM2_E_NO_BAD_BLOCK_FOUND;
	else
		expected_result = PMEM2_E_INVALID_FILE_TYPE;

	int result = test_func(&src);

	UT_ASSERTeq(result, expected_result);

	DONE(NULL);
}
