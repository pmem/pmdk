// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmemset_badblock.c -- pmemset bad block tests
 */

#include "libpmemset.h"
#include "unittest.h"
#include "ut_pmemset_utils.h"

#include <errno.h>

/*
 * test_pmemset_src_mcsafe_badblock_read -- test mcsafe read operation with
 *                                          encountered badblock
 */
static int
test_pmemset_src_mcsafe_badblock_read(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_pmemset_src_mcsafe_badblock_read <file>");

	char *file = argv[0];
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);
	ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_IO_FAIL);

	FREE(buf);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_pmemset_src_mcsafe_badblock_write -- test mcsafe write operation with
 *                                           encountered badblock
 */
static int
test_pmemset_src_mcsafe_badblock_write(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmemset_src_mcsafe_badblock_write <file>");

	char *file = argv[0];
	struct pmemset_source *src;

	int ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	size_t bufsize = 4096;
	void *buf = MALLOC(bufsize);
	memset(buf, '6', bufsize);
	ret = pmemset_source_pread_mcsafe(src, buf, bufsize, 0);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_IO_FAIL);

	FREE(buf);

	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_pmemset_map_detect_badblock -- test pmemset map on a source with a
 *                                     badblock
 */
static int
test_pmemset_map_detect_badblock(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmemset_map_detect_badblock <file>");

	char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_source *src;

	ut_create_set_config(&cfg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_set_badblock_detection(src, true);

	ret = pmemset_map(set, src, NULL, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, PMEMSET_E_IO_FAIL);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

struct badblock_event_arg {
	size_t n_bb_found;
	size_t n_bb_all_clear;
};

/*
 * badblock_event_cb -- callback for events involving badblocks
 */
static int
badblock_event_cb(struct pmemset *set, struct pmemset_event_context *ctx,
		void *arg)
{
	struct badblock_event_arg *args = (struct badblock_event_arg *)arg;

	if (ctx->type == PMEMSET_EVENT_BADBLOCK) {
		struct pmemset_badblock *bb = ctx->data.badblock.bb;
		struct pmemset_source *src = ctx->data.badblock.src;

		int ret = pmemset_badblock_clear(bb, src);
		UT_PMEMSET_EXPECT_RETURN(ret, 0);

		args->n_bb_found += 1;
	} else if (ctx->type == PMEMSET_EVENT_BADBLOCKS_CLEARED) {
		args->n_bb_all_clear += 1;
	}

	return 0;
}

/*
 * test_pmemset_map_detect_badblock_and_clear -- test pmemset map on a source
 *                                               with a badblock and clear it
 */
static int
test_pmemset_map_detect_badblock_and_clear(const struct test_case *tc, int argc,
		char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_pmemset_map_detect_badblock_and_clear "
			"<file>");

	char *file = argv[0];
	struct pmemset *set;
	struct pmemset_config *cfg;
	struct pmemset_map_config *map_cfg;
	struct pmemset_source *src;
	struct badblock_event_arg arg = { .n_bb_found = 0, .n_bb_all_clear = 0};

	ut_create_set_config(&cfg);
	pmemset_config_set_event_callback(cfg, badblock_event_cb, &arg);

	int ret = pmemset_new(&set, cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	ret = pmemset_source_from_file(&src, file);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	pmemset_source_set_badblock_detection(src, true);

	ut_create_map_config(&map_cfg, 0, (1 << 22));

	/* bad block should get cleared by the callback */
	ret = pmemset_map(set, src, map_cfg, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* callback fired 2 times, 1 badblock found + 1 all bb cleared */
	UT_ASSERTeq(arg.n_bb_found, 1);
	UT_ASSERTeq(arg.n_bb_all_clear, 1);

	ret = pmemset_map(set, src, map_cfg, NULL);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	/* callback not fired, no bad blocks found */
	UT_ASSERTeq(arg.n_bb_found, 1);
	UT_ASSERTeq(arg.n_bb_all_clear, 1);

	ret = pmemset_delete(&set);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_config_delete(&cfg);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);
	ret = pmemset_source_delete(&src);
	UT_PMEMSET_EXPECT_RETURN(ret, 0);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_pmemset_src_mcsafe_badblock_read),
	TEST_CASE(test_pmemset_src_mcsafe_badblock_write),
	TEST_CASE(test_pmemset_map_detect_badblock),
	TEST_CASE(test_pmemset_map_detect_badblock_and_clear),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmemset_badblock");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
