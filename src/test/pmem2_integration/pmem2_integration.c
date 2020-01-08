/*
 * Copyright 2019-2020, Intel Corporation
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
 * pmem2_integration.c -- pmem2 integration tests
 */

#include "unittest.h"
#include "ut_pmem2_utils.h"

#define N_GRANULARITIES 3 /* BYTE, CACHE_LINE, PAGE */

/*
 * prepare_config -- fill pmem2_config in minimal scope
 */
static void
prepare_config(struct pmem2_config **cfg, int fd,
		enum pmem2_granularity granularity)
{
	int ret = pmem2_config_new(cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	if (fd != -1) {
		ret = pmem2_config_set_fd(*cfg, fd);
		UT_PMEM2_EXPECT_RETURN(ret, 0);
	}

	ret = pmem2_config_set_required_store_granularity(*cfg, granularity);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
}

/*
 * map_invalid -- try to mapping memory with invalid config
 */
static void
map_invalid(struct pmem2_config *cfg, int result)
{
	struct pmem2_map *map = (struct pmem2_map *)0x7;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, result);
	UT_ASSERTeq(map, NULL);
}

/*
 * map_valid -- return valid mapped pmem2_map and validate mapped memory length
 */
static struct pmem2_map *
map_valid(struct pmem2_config *cfg, size_t size)
{
	struct pmem2_map *map = NULL;
	int ret = pmem2_map(cfg, &map);
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
	prepare_config(&cfg, fd, PMEM2_GRANULARITY_PAGE);

	size_t size;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size), 0);

	struct pmem2_map *map1 = map_valid(cfg, size);
	struct pmem2_map *map2 = map_valid(cfg, size);

	/* cleanup after the test */
	pmem2_unmap(&map2);
	pmem2_unmap(&map1);
	pmem2_config_delete(&cfg);
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
	prepare_config(&cfg, fd1, PMEM2_GRANULARITY_PAGE);

	size_t size1;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size1), 0);

	struct pmem2_map *map1 = map_valid(cfg, size1);

	char *file2 = argv[1];
	int fd2 = OPEN(file2, O_RDWR);

	/* set another valid file descriptor in config */
	int ret = pmem2_config_set_fd(cfg, fd2);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	size_t size2;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size2), 0);

	struct pmem2_map *map2 = map_valid(cfg, size2);

	/* cleanup after the test */
	pmem2_unmap(&map2);
	CLOSE(fd2);
	pmem2_unmap(&map1);
	pmem2_config_delete(&cfg);
	CLOSE(fd1);

	return 2;
}

/*
 * test_dafault_fd -- try to map pmem2_map using the pmem2_config with default
 * file descriptor
 */
static int
test_default_fd(const struct test_case *tc, int argc, char *argv[])
{
	struct pmem2_config *cfg;
	/* set invalid file descriptor in config */
	prepare_config(&cfg, -1, PMEM2_GRANULARITY_PAGE);

	map_invalid(cfg, PMEM2_E_FILE_HANDLE_NOT_SET);

	/* cleanup after the test */
	pmem2_config_delete(&cfg);

	return 0;
}

/*
 * test_invalid_fd -- try to change pmem2_config (unsuccessful) and
 * map pmem2_map again
 */
static int
test_invalid_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_invalid_fd <file> <file2>");

	char *file1 = argv[0];
	int fd1 = OPEN(file1, O_RDWR);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd1, PMEM2_GRANULARITY_PAGE);

	size_t size1;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size1), 0);

	struct pmem2_map *map1 = map_valid(cfg, size1);

	char *file2 = argv[1];
	int fd2 = OPEN(file2, O_WRONLY);

	/* attempt to set O_WRONLY file descriptor - expect failure */
	int ret = pmem2_config_set_fd(cfg, fd2);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	struct pmem2_map *map2 = map_valid(cfg, size1);

	/* cleanup after the test */
	pmem2_unmap(&map2);
	CLOSE(fd2);
	pmem2_unmap(&map1);
	pmem2_config_delete(&cfg);
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
	prepare_config(&cfg, fd, PMEM2_GRANULARITY_PAGE);

	size_t size;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &size), 0);

	struct pmem2_map *map = map_valid(cfg, size);

	char *addr = pmem2_map_get_address(map);
	size_t length = strlen(word);
	/* write some data in mapped memory without persisting data */
	memcpy(addr, word, length);

	/* cleanup after the test */
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
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
	prepare_config(&cfg, fd, PMEM2_GRANULARITY_PAGE);

	size_t len;
	UT_ASSERTeq(pmem2_config_get_file_size(cfg, &len), 0);

	struct pmem2_map *map = map_valid(cfg, len);
	char *base = pmem2_map_get_address(map);

	unsigned seed = 13; /* arbitrarily chosen value */
	for (size_t i = 0; i < len; i++)
		base[i] = os_rand_r(&seed);

	UT_ASSERTeq(len % Ut_mmap_align, 0);
	for (size_t l = len; l > 0; l -= Ut_mmap_align) {
		for (size_t off = 0; off < l; off += Ut_mmap_align) {
			size_t len2 = l - off;
			int ret = pmem2_config_set_length(cfg, len2);
			UT_PMEM2_EXPECT_RETURN(ret, 0);

			ret = pmem2_config_set_offset(cfg, off);
			UT_PMEM2_EXPECT_RETURN(ret, 0);

			struct pmem2_map *map2 = map_valid(cfg, len2);
			char *ptr = pmem2_map_get_address(map2);

			UT_ASSERTeq(ret = memcmp(base + off, ptr, len2), 0);
			pmem2_unmap(&map2);
		}
	}
	pmem2_unmap(&map);
	pmem2_config_delete(&cfg);
	CLOSE(fd);
	return 1;
}

struct gran_test_ctx;

typedef void(*map_func)(struct pmem2_config *cfg, struct gran_test_ctx *ctx);

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
map_with_avail_gran(struct pmem2_config *cfg, struct gran_test_ctx *ctx)
{
	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
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
map_with_unavail_gran(struct pmem2_config *cfg, struct gran_test_ctx *unused)
{
	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
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
	prepare_config(&cfg, fd, gran_id2granularity[req_gran_id]);

	ctx.map_with_expected_gran(cfg, &ctx);

	pmem2_config_delete(&cfg);
	CLOSE(fd);

	return 3;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_reuse_cfg),
	TEST_CASE(test_reuse_cfg_with_diff_fd),
	TEST_CASE(test_default_fd),
	TEST_CASE(test_invalid_fd),
	TEST_CASE(test_register_pmem),
	TEST_CASE(test_use_misc_lens_and_offsets),
	TEST_CASE(test_granularity),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_integration");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
