// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

/*
 * pmem2_integration.c -- pmem2 integration tests
 */

#include "libpmem2.h"
#include "unittest.h"
#include "rand.h"
#include "ut_pmem2.h"
#include "ut_pmem2_setup_integration.h"

#define N_GRANULARITIES 3 /* BYTE, CACHE_LINE, PAGE */

/*
 * map_invalid -- try to mapping memory with invalid config
 */
static void
map_invalid(struct pmem2_config *cfg, struct pmem2_source *src, int result)
{
	struct pmem2_map *map = (struct pmem2_map *)0x7;
	int ret = pmem2_map_new(&map, cfg, src);
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
	int ret = pmem2_map_new(&map, cfg, src);
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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t size;
	UT_ASSERTeq(pmem2_source_size(src, &size), 0);

	struct pmem2_map *map1 = map_valid(cfg, src, size);
	struct pmem2_map *map2 = map_valid(cfg, src, size);

	/* cleanup after the test */
	pmem2_map_delete(&map2);
	pmem2_map_delete(&map1);
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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd1,
						PMEM2_GRANULARITY_PAGE);

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
	pmem2_map_delete(&map2);
	CLOSE(fd2);
	pmem2_map_delete(&map1);
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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t size;
	UT_ASSERTeq(pmem2_source_size(src, &size), 0);

	struct pmem2_map *map = map_valid(cfg, src, size);

	char *addr = pmem2_map_get_address(map);
	size_t length = strlen(word);
	/* write some data in mapped memory without persisting data */
	memcpy(addr, word, length);

	/* cleanup after the test */
	pmem2_map_delete(&map);
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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len;
	UT_ASSERTeq(pmem2_source_size(src, &len), 0);

	struct pmem2_map *map = map_valid(cfg, src, len);
	char *base = pmem2_map_get_address(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);

	rng_t rng;
	randomize_r(&rng, 13); /* arbitrarily chosen value */
	for (size_t i = 0; i < len; i++)
		base[i] = (char)rnd64_r(&rng);
	persist_fn(base, len);

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
			pmem2_map_delete(&map2);
		}
	}
	pmem2_map_delete(&map);
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
	int ret = pmem2_map_new(&map, cfg, src);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
	UT_ASSERTne(map, NULL);
	UT_ASSERTeq(ctx->expected_granularity,
			pmem2_map_get_store_granularity(map));

	/* cleanup after the test */
	pmem2_map_delete(&map);
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
	int ret = pmem2_map_new(&map, cfg, src);
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
 * test_granularity -- performs pmem2_map_new with certain expected granularity
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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
					gran_id2granularity[req_gran_id]);

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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len, alignment;
	int ret = pmem2_source_size(src, &len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	PMEM2_SOURCE_ALIGNMENT(src, &alignment);

	UT_ASSERT(len > alignment);
	size_t aligned_len = ALIGN_DOWN(len, alignment);

	ret = pmem2_config_set_length(cfg, aligned_len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	struct pmem2_map *map = map_valid(cfg, src, aligned_len);

	pmem2_map_delete(&map);
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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

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
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

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

	pmem2_map_delete(&map);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_mem_move_cpy_set_with_map_private -- map O_RDONLY file and do
 * pmem2_[cpy|set|move]_fns with PMEM2_PRIVATE sharing
 */
static int
test_mem_move_cpy_set_with_map_private(const struct test_case *tc, int argc,
					char *argv[])
{
	if (argc < 1)
		UT_FATAL(
			"usage: test_mem_move_cpy_set_with_map_private <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDONLY);
	const char *word1 = "Persistent memory...";
	const char *word2 = "Nonpersistent memory";
	const char *word3 = "XXXXXXXXXXXXXXXXXXXX";

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);
	pmem2_config_set_sharing(cfg, PMEM2_PRIVATE);

	size_t size = 0;
	UT_ASSERTeq(pmem2_source_size(src, &size), 0);
	struct pmem2_map *map = map_valid(cfg, src, size);

	char *addr = pmem2_map_get_address(map);

	/* copy inital state */
	char *initial_state = MALLOC(size);
	memcpy(initial_state, addr, size);

	pmem2_memmove_fn memmove_fn = pmem2_get_memmove_fn(map);
	pmem2_memcpy_fn memcpy_fn = pmem2_get_memcpy_fn(map);
	pmem2_memset_fn memset_fn = pmem2_get_memset_fn(map);

	memcpy_fn(addr, word1, strlen(word1), 0);
	UT_ASSERTeq(strcmp(addr, word1), 0);

	memmove_fn(addr, word2, strlen(word2), 0);
	UT_ASSERTeq(strcmp(addr, word2), 0);

	memset_fn(addr, 'X', strlen(word3), 0);
	UT_ASSERTeq(strcmp(addr, word3), 0);

	/* remap memory, and check that the data has not been saved */
	pmem2_map_delete(&map);
	map = map_valid(cfg, src, size);
	addr = pmem2_map_get_address(map);
	UT_ASSERTeq(strcmp(addr, initial_state), 0);

	/* cleanup after the test */
	pmem2_map_delete(&map);
	FREE(initial_state);
	pmem2_config_delete(&cfg);
	pmem2_source_delete(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_deep_flush_valid -- perform valid deep_flush for whole map
 */
static int
test_deep_flush_valid(const struct test_case *tc, int argc, char *argv[])
{
	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len;
	PMEM2_SOURCE_SIZE(src, &len);

	struct pmem2_map *map = map_valid(cfg, src, len);

	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	memset(addr, 0, len);
	persist_fn(addr, len);

	int ret = pmem2_deep_flush(map, addr, len);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	pmem2_map_delete(&map);
	PMEM2_CONFIG_DELETE(&cfg);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_deep_flush_e_range_behind -- try deep_flush for range behind a map
 */
static int
test_deep_flush_e_range_behind(const struct test_case *tc,
	int argc, char *argv[])
{
	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len;
	PMEM2_SOURCE_SIZE(src, &len);

	struct pmem2_map *map = map_valid(cfg, src, len);

	size_t map_size = pmem2_map_get_size(map);
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	memset(addr, 0, len);
	persist_fn(addr, len);

	int ret = pmem2_deep_flush(map, addr + map_size + 1, 64);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_DEEP_FLUSH_RANGE);

	pmem2_map_delete(&map);
	PMEM2_CONFIG_DELETE(&cfg);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_deep_flush_e_range_before -- try deep_flush for range before a map
 */
static int
test_deep_flush_e_range_before(const struct test_case *tc,
	int argc, char *argv[])
{
	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len;
	PMEM2_SOURCE_SIZE(src, &len);

	struct pmem2_map *map = map_valid(cfg, src, len);

	size_t map_size = pmem2_map_get_size(map);
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	memset(addr, 0, len);
	persist_fn(addr, len);

	int ret = pmem2_deep_flush(map, addr - map_size, 64);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_DEEP_FLUSH_RANGE);

	pmem2_map_delete(&map);
	PMEM2_CONFIG_DELETE(&cfg);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_deep_flush_slice -- try deep_flush for slice of a map
 */
static int
test_deep_flush_slice(const struct test_case *tc, int argc, char *argv[])
{
	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len;
	PMEM2_SOURCE_SIZE(src, &len);

	struct pmem2_map *map = map_valid(cfg, src, len);

	size_t map_size = pmem2_map_get_size(map);
	size_t map_part = map_size / 4;
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	memset(addr, 0, map_part);
	persist_fn(addr, map_part);

	int ret = pmem2_deep_flush(map, addr + map_part, map_part);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	pmem2_map_delete(&map);
	PMEM2_CONFIG_DELETE(&cfg);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_deep_flush_overlap -- try deep_flush for range overlaping map
 */
static int
test_deep_flush_overlap(const struct test_case *tc, int argc, char *argv[])
{
	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	struct pmem2_source *src;
	PMEM2_PREPARE_CONFIG_INTEGRATION(&cfg, &src, fd,
						PMEM2_GRANULARITY_PAGE);

	size_t len;
	PMEM2_SOURCE_SIZE(src, &len);

	struct pmem2_map *map = map_valid(cfg, src, len);

	size_t map_size = pmem2_map_get_size(map);
	char *addr = pmem2_map_get_address(map);
	pmem2_persist_fn persist_fn = pmem2_get_persist_fn(map);
	memset(addr, 0, len);
	persist_fn(addr, len);

	int ret = pmem2_deep_flush(map, addr + 1024, map_size);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_DEEP_FLUSH_RANGE);

	pmem2_map_delete(&map);
	PMEM2_CONFIG_DELETE(&cfg);
	PMEM2_SOURCE_DELETE(&src);
	CLOSE(fd);

	return 1;
}

/*
 * test_source_anon -- tests map/config/source functions in combination
 *	with anonymous source.
 */
static int
test_source_anon(enum pmem2_sharing_type sharing,
	enum pmem2_granularity granularity,
	size_t source_len, size_t map_len)
{
	int ret = 0;
	struct pmem2_config *cfg;
	struct pmem2_source *src;
	struct pmem2_map *map;
	struct pmem2_badblock_context *bbctx;

	UT_ASSERTeq(pmem2_source_from_anon(&src, source_len), 0);

	UT_ASSERTeq(pmem2_source_device_id(src, NULL, NULL), PMEM2_E_NOSUPP);
	UT_ASSERTeq(pmem2_source_device_usc(src, NULL), PMEM2_E_NOSUPP);
	UT_ASSERTeq(pmem2_badblock_context_new(&bbctx, src), PMEM2_E_NOSUPP);
	size_t alignment;
	UT_ASSERTeq(pmem2_source_alignment(src, &alignment), 0);
	UT_ASSERT(alignment >= Ut_pagesize);
	size_t size;
	UT_ASSERTeq(pmem2_source_size(src, &size), 0);
	UT_ASSERTeq(size, source_len);

	PMEM2_CONFIG_NEW(&cfg);

	UT_ASSERTeq(pmem2_config_set_length(cfg, map_len), 0);
	UT_ASSERTeq(pmem2_config_set_offset(cfg, alignment), 0); /* ignored */
	UT_ASSERTeq(pmem2_config_set_required_store_granularity(cfg,
		granularity), 0);
	UT_ASSERTeq(pmem2_config_set_sharing(cfg, sharing), 0);

	if ((ret = pmem2_map_new(&map, cfg, src)) != 0)
		goto map_fail;

	void *addr = pmem2_map_get_address(map);
	UT_ASSERTne(addr, NULL);
	UT_ASSERTeq(pmem2_map_get_size(map), map_len ? map_len : source_len);
	UT_ASSERTeq(pmem2_map_get_store_granularity(map),
		PMEM2_GRANULARITY_BYTE);

	UT_ASSERTeq(pmem2_deep_flush(map, addr, alignment), PMEM2_E_NOSUPP);

	UT_ASSERTeq(pmem2_map_delete(&map), 0);

map_fail:
	PMEM2_CONFIG_DELETE(&cfg);
	pmem2_source_delete(&src);

	return ret;
}

/*
 * test_source_anon_ok_private -- valid config /w private flag
 */
static int
test_source_anon_private(const struct test_case *tc, int argc, char *argv[])
{
	int ret = test_source_anon(PMEM2_PRIVATE, PMEM2_GRANULARITY_BYTE,
		1 << 30ULL, 1 << 20ULL);
	UT_ASSERTeq(ret, 0);

	return 1;
}

/*
 * test_source_anon_shared -- valid config /w shared flag
 */
static int
test_source_anon_shared(const struct test_case *tc, int argc, char *argv[])
{
	int ret = test_source_anon(PMEM2_SHARED, PMEM2_GRANULARITY_BYTE,
		1 << 30ULL, 1 << 20ULL);
	UT_ASSERTeq(ret, 0);

	return 1;
}

/*
 * test_source_anon_page -- valid config /w page granularity
 */
static int
test_source_anon_page(const struct test_case *tc, int argc, char *argv[])
{
	int ret = test_source_anon(PMEM2_SHARED, PMEM2_GRANULARITY_PAGE,
		1 << 30ULL, 1 << 20ULL);
	UT_ASSERTeq(ret, 0);

	return 1;
}

/*
 * test_source_anon_zero_len -- valid config /w zero (src inherited) map length
 */
static int
test_source_anon_zero_len(const struct test_case *tc, int argc, char *argv[])
{
	int ret = test_source_anon(PMEM2_SHARED, PMEM2_GRANULARITY_BYTE,
		1 << 30ULL, 0);
	UT_ASSERTeq(ret, 0);

	return 1;
}

/*
 * test_source_anon_too_small -- valid config /w small mapping length
 */
static int
test_source_anon_too_small(const struct test_case *tc, int argc, char *argv[])
{
	int ret = test_source_anon(PMEM2_SHARED, PMEM2_GRANULARITY_BYTE,
		1 << 30ULL, 1 << 10ULL);
	UT_ASSERTne(ret, 0);

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
	TEST_CASE(test_mem_move_cpy_set_with_map_private),
	TEST_CASE(test_deep_flush_valid),
	TEST_CASE(test_deep_flush_e_range_behind),
	TEST_CASE(test_deep_flush_e_range_before),
	TEST_CASE(test_deep_flush_slice),
	TEST_CASE(test_deep_flush_overlap),
	TEST_CASE(test_source_anon_private),
	TEST_CASE(test_source_anon_shared),
	TEST_CASE(test_source_anon_page),
	TEST_CASE(test_source_anon_too_small),
	TEST_CASE(test_source_anon_zero_len),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_integration");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
