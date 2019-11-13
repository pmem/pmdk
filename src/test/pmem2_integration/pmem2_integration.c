/*
 * Copyright 2019, Intel Corporation
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

/*
 * prepare_config -- fill pmem2_config in minimal scope
 */
static void
prepare_config(struct pmem2_config **cfg, int fd)
{
	int ret = pmem2_config_new(cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	if (fd != -1) {
		ret = pmem2_config_set_fd(*cfg, fd);
		UT_PMEM2_EXPECT_RETURN(ret, 0);
	}

	ret = pmem2_config_set_required_store_granularity(*cfg,
		PMEM2_GRANULARITY_PAGE);
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
	if (argc != 1)
		UT_FATAL("usage: test_reuse_cfg <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd);

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
	if (argc != 2)
		UT_FATAL("usage: test_reuse_cfg_with_diff_fd <file> <file2>");

	char *file1 = argv[0];
	int fd1 = OPEN(file1, O_RDWR);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd1);

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
	if (argc != 0)
		UT_FATAL("usage: test_reuse_cfg_with_diff_fd");

	struct pmem2_config *cfg;
	/* set invalid file descriptor in config */
	prepare_config(&cfg, -1);

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
	if (argc != 2)
		UT_FATAL("usage: test_invalid_fd <file> <file2>");

	char *file1 = argv[0];
	int fd1 = OPEN(file1, O_RDWR);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd1);

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
	if (argc != 1)
		UT_FATAL("usage: test_register_pmem <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	char *word = "XXXXXXXX";

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd);

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
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_reuse_cfg),
	TEST_CASE(test_reuse_cfg_with_diff_fd),
	TEST_CASE(test_default_fd),
	TEST_CASE(test_invalid_fd),
	TEST_CASE(test_register_pmem),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_integration");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
