// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_integration.c -- pmem2 integration tests
 */

#include "unittest.h"
#include "rand.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"

#define N_GRANULARITIES 3 /* BYTE, CACHE_LINE, PAGE */

/*
 * prepare_config -- fill pmem2_config in minimal scope
 */
static void
prepare_config(struct pmem2_config **cfg, struct pmem2_source **src, int fd,
		enum pmem2_granularity granularity)
{
	int ret = pmem2_config_new(cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	if (fd != -1) {
		ret = pmem2_source_from_fd(src, fd);
		UT_PMEM2_EXPECT_RETURN(ret, 0);
	}

	ret = pmem2_config_set_required_store_granularity(*cfg, granularity);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
}

/*
 * map_invalid -- try to mapping memory with invalid config
 */
static void
map_invalid(struct pmem2_config *cfg, struct pmem2_source *src, int result)
{
	struct pmem2_map *map = (struct pmem2_map *)0x7;
	int ret = pmem2_map(cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, result);
	UT_ASSERTeq(map, NULL);
}

/*
 * map_valid -- return valid mapped pmem2_map and validate mapped memory length
 */
static struct pmem2_map *
map_valid(struct pmem2_config *cfg, struct pmem2_source *src, size_t size)
{
	struct pmem2_map *map = NULL;
	int ret = pmem2_map(cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map, NULL);
	UT_ASSERTeq(pmem2_map_get_size(map), size);

	return map;
}

/*
 * test_reuse_cfg -- map pmem2_map twice using the same pmem2_config
 */
static int
test_reuse_cfg(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_reuse_cfg <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t size;
	UT_ASSERTeq(pmem2_source_size(src, &size), 0);

	struct pmem2_map *map1 = map_valid(cfg, src, size);
	struct pmem2_map *map2 = map_valid(cfg, src, size);

	/* cleanup after the test */
	pmem2_unmap(&map2);
	pmem2_unmap(&map1);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_reuse_cfg_with_diff_fd -- map pmem2_map using the same pmem2_config
 * with changed file descriptor
 */
static int
test_reuse_cfg_with_diff_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_reuse_cfg_with_diff_fd <file> <file2>");

	char *file1 = argv[0];
	int fd1 = OPEN(file1, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd1, PMEM2_GRANULARITY_PAGE);

	size_t size1;
	UT_ASSERTeq(pmem2_source_size(src, &size1), 0);

	struct pmem2_map *map1 = map_valid(cfg, src, size1);

	char *file2 = argv[1];
	int fd2 = OPEN(file2, O_RDWR);

	/* set another valid file descriptor in source */
	struct pmem2_source *src2;
	UT_ASSERTeq(pmem2_source_from_fd(&src2, fd2), 0);

	size_t size2;
	UT_ASSERTeq(pmem2_source_size(src2, &size2), 0);

	struct pmem2_map *map2 = map_valid(cfg, src2, size2);

	/* cleanup after the test */
	pmem2_unmap(&map2);
	CLOSE(fd2);
	pmem2_unmap(&map1);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	pmem2_source_delete(&src2);
	CLOSE(fd1);

	return 2;
}

/*
 * test_register_pmem -- map, use and unmap memory
 */
static int
test_register_pmem(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_register_pmem <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	char *word = "XXXXXXXX";

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t size;
	UT_ASSERTeq(pmem2_source_size(src, &size), 0);

	struct pmem2_map *map = map_valid(cfg, src, size);

	char *addr = pmem2_map_get_address(map);
	size_t length = strlen(word);
	/* write some data in mapped memory without persisting data */
	memcpy(addr, word, length);

	/* cleanup after the test */
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_use_misc_lens_and_offsets -- test with multiple offsets and lengths
 */
static int
test_use_misc_lens_and_offsets(const struct test_case *tc,
	int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_use_misc_lens_and_offsets <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t len;
	UT_ASSERTeq(pmem2_source_size(src, &len), 0);

	struct pmem2_map *map = map_valid(cfg, src, len);
	char *base = pmem2_map_get_address(map);

	rng_t rng;
	randomize_r(&rng, 13); /* arbitrarily chosen value */
	for (size_t i = 0; i < len; i++)
		base[i] = (char)rnd64_r(&rng);

	UT_ASSERTeq(len % Ut_mmap_align, 0);
	for (size_t l = len; l > 0; l -= Ut_mmap_align) {
		for (size_t off = 0; off < l; off += Ut_mmap_align) {
			size_t len2 = l - off;
			int ret = pmem2_config_set_length(cfg, len2);
			UT_PMEM2_EXPECT_RETURN(ret, 0);

			ret = pmem2_config_set_offset(cfg, off);
			UT_PMEM2_EXPECT_RETURN(ret, 0);

			struct pmem2_map *map2 = map_valid(cfg, src, len2);
			char *ptr = pmem2_map_get_address(map2);

			UT_ASSERTeq(ret = memcmp(base + off, ptr, len2), 0);
			pmem2_unmap(&map2);
		}
	}
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);
	return 1;
}

struct gran_test_ctx;

typedef void(*map_func)(struct pmem2_config *cfg,
	struct pmem2_source *src, struct gran_test_ctx *ctx);

/*
 * gran_test_ctx -- essential parameters used by granularity test
 */
struct gran_test_ctx {
	map_func map_with_expected_gran;
	enum pmem2_granularity expected_granularity;
};

/*
 * map_with_avail_gran -- map the range with valid granularity,
 * includes cleanup
 */
static void
map_with_avail_gran(struct pmem2_config *cfg,
	struct pmem2_source *src, struct gran_test_ctx *ctx)
{
	struct pmem2_map *map;
	int ret = pmem2_map(cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map, NULL);
	UT_ASSERTeq(ctx->expected_granularity,
			pmem2_map_get_store_granularity(map));

	/* cleanup after the test */
	pmem2_unmap(&map);
}

/*
 * map_with_unavail_gran -- map the range with invalid granularity
 * (unsuccessful)
 */
static void
map_with_unavail_gran(struct pmem2_config *cfg,
	struct pmem2_source *src, struct gran_test_ctx *unused)
{
	struct pmem2_map *map;
	int ret = pmem2_map(cfg, src, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_GRANULARITY_NOT_SUPPORTED);
	UT_ERR("%s", pmem2_errormsg());
	UT_ASSERTeq(map, NULL);
}

static const map_func map_with_gran[N_GRANULARITIES][N_GRANULARITIES] = {
/*		requested granularity / available granularity		*/
/* -------------------------------------------------------------------- */
/*		BYTE		CACHE_LINE		PAGE		*/
/* -------------------------------------------------------------------- */
/* BYTE */ {map_with_avail_gran, map_with_unavail_gran, map_with_unavail_gran},
/* CL	*/ {map_with_avail_gran, map_with_avail_gran,   map_with_unavail_gran},
/* PAGE */ {map_with_avail_gran, map_with_avail_gran,   map_with_avail_gran}};

static const enum pmem2_granularity gran_id2granularity[N_GRANULARITIES] = {
						PMEM2_GRANULARITY_BYTE,
						PMEM2_GRANULARITY_CACHE_LINE,
						PMEM2_GRANULARITY_PAGE};

/*
 * str2gran_id -- reads granularity id from the provided string
 */
static int
str2gran_id(const char *in)
{
	int gran = atoi(in);
	UT_ASSERT(gran >= 0 && gran < N_GRANULARITIES);

	return gran;
}

/*
 * test_granularity -- performs pmem2_map with certain expected granularity
 * in context of certain available granularity
 */
static int
test_granularity(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 3)
		UT_FATAL(
		"usage: test_granularity <file>"
				" <available_granularity> <requested_granularity>");

	struct gran_test_ctx ctx;

	int avail_gran_id = str2gran_id(argv[1]);
	int req_gran_id = str2gran_id(argv[2]);

	ctx.expected_granularity = gran_id2granularity[avail_gran_id];
	ctx.map_with_expected_gran = map_with_gran[req_gran_id][avail_gran_id];

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, gran_id2granularity[req_gran_id]);

	ctx.map_with_expected_gran(cfg, src, &ctx);

	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 3;
}

/*
 * test_len_not_aligned -- try to use unaligned length
 */
static int
test_len_not_aligned(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_len_not_aligned <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t len, alignment;
	int ret = pmem2_source_size(src, &len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	UT_ASSERT(len > alignment);
	size_t aligned_len = ALIGN_DOWN(len, alignment);
	size_t unaligned_len = aligned_len - 1;

	ret = pmem2_config_set_length(cfg, unaligned_len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	map_invalid(cfg, src, PMEM2_E_LENGTH_UNALIGNED);

	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);

	CLOSE(fd);

	return 1;
}

/*
 * test_len_aligned -- try to use aligned length
 */
static int
test_len_aligned(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_len_aligned <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t len, alignment;
	int ret = pmem2_source_size(src, &len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	UT_ASSERT(len > alignment);
	size_t aligned_len = ALIGN_DOWN(len, alignment);

	ret = pmem2_config_set_length(cfg, aligned_len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	struct pmem2_map *map = map_valid(cfg, src, aligned_len);

	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);

	CLOSE(fd);

	return 1;
}

/*
 * test_offset_not_aligned -- try to map with unaligned offset
 */
static int
test_offset_not_aligned(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_offset_not_aligned <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t len, alignment;
	int ret = pmem2_source_size(src, &len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	/* break the offset */
	size_t offset = alignment - 1;
	ret = pmem2_config_set_offset(cfg, offset);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERT(len > alignment);
	/* in this case len has to be aligned, only offset will be unaligned */
	size_t aligned_len = ALIGN_DOWN(len, alignment);

	ret = pmem2_config_set_length(cfg, aligned_len - alignment);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	map_invalid(cfg, src, PMEM2_E_OFFSET_UNALIGNED);

	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);

	CLOSE(fd);

	return 1;
}

/*
 * test_offset_aligned -- try to map with aligned offset
 */
static int
test_offset_aligned(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_offset_aligned <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	prepare_config(&cfg, &src, fd, PMEM2_GRANULARITY_PAGE);

	size_t len, alignment;
	int ret = pmem2_source_size(src, &len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	/* set the aligned offset */
	size_t offset = alignment;
	ret = pmem2_config_set_offset(cfg, offset);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	UT_ASSERT(len > alignment * 2);
	/* set the aligned len */
	size_t map_len = ALIGN_DOWN(len / 2, alignment);
	ret = pmem2_config_set_length(cfg, map_len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	struct pmem2_map *map = map_valid(cfg, src, map_len);

	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_reuse_cfg),
	TEST_CASE(test_reuse_cfg_with_diff_fd),
	TEST_CASE(test_register_pmem),
	TEST_CASE(test_use_misc_lens_and_offsets),
	TEST_CASE(test_granularity),
	TEST_CASE(test_len_not_aligned),
	TEST_CASE(test_len_aligned),
	TEST_CASE(test_offset_not_aligned),
	TEST_CASE(test_offset_aligned),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_integration");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
