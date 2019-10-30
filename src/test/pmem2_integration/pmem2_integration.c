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
 * pmem2_integration.c -- pmem2_integration tests
 */

#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"

/*
 * get_size -- get a file size
 */
static size_t
get_size(char *file)
{
	os_stat_t stbuf;
	STAT(file, &stbuf);

	return (size_t)stbuf.st_size;
}

/*
 * prepare_config -- fill pmem2_config in minimal scope
 */
static void
prepare_config(struct pmem2_config **cfg, int fd)
{
	int ret = pmem2_config_new(cfg);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	ret = pmem2_config_set_fd(*cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
}

/*
 * map_invalid -- try to mapping memory with invalid config
 */
static void
map_invalid(struct pmem2_config *cfg, int result)
{
	struct pmem2_map *map = NULL;
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
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
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
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	size_t size = get_size(file);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd);

	struct pmem2_map *map = map_valid(cfg, size);
	struct pmem2_map *map2 = map_valid(cfg, size);

	/* Cleanup after the test */
	pmem2_config_delete(&cfg);
	pmem2_unmap(&map);
	pmem2_unmap(&map2);
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
		UT_FATAL("usage: %s test_case file file2", argv[0]);

	char *file = argv[0];
	size_t size = get_size(file);
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd);

	struct pmem2_map *map = map_valid(cfg, size);

	char *file2 = argv[1];
	size_t size2 = get_size(file2);
	int fd2 = OPEN(file2, O_RDWR);

	/* Set another valid file descriptor in config */
	int ret = pmem2_config_set_fd(cfg, fd2);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	struct pmem2_map *map2 = map_valid(cfg, size2);

	/* Set invalid file descriptor in config */
	ret = pmem2_config_set_fd(cfg, -1);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	map_invalid(cfg, PMEM2_E_INVALID_FILE_HANDLE);

	/* Cleanup after the test */
	pmem2_config_delete(&cfg);
	pmem2_unmap(&map);
	pmem2_unmap(&map2);
	CLOSE(fd);
	CLOSE(fd2);

	return 2;
}

/*
 * test_invalid_fd -- try to change pmem2_config (failure) and
 * map pmem2_map again
 */
static int
test_invalid_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s test_case file file2", argv[0]);

	char *file = argv[0];
	size_t size = get_size(file);
	int fd = OPEN(file, O_RDWR);

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd);

	struct pmem2_map *map = map_valid(cfg, size);

	char *file2 = argv[1];
	int fd2 = OPEN(file2, O_WRONLY);

	/* attempt to change file descriptor in config - expect failure */
	int ret = pmem2_config_set_fd(cfg, fd2);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_FILE_HANDLE);
	struct pmem2_map *map2 = map_valid(cfg, size);

	/* Cleanup after the test */
	pmem2_config_delete(&cfg);
	pmem2_unmap(&map);
	pmem2_unmap(&map2);
	CLOSE(fd);
	CLOSE(fd2);

	return 2;
}

/*
 * test_register_pmem -- map, use and unmap memory
 */
static int
test_register_pmem(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	size_t size = get_size(file);
	int fd = OPEN(file, O_RDWR);
	char *word = "XXXXXXXX";

	struct pmem2_config *cfg;
	prepare_config(&cfg, fd);

	struct pmem2_map *map = map_valid(cfg, size);

	char *addr = pmem2_map_get_address(map);
	size_t length = sizeof(word);
	/* Write some data in mapped memory without persisting data */
	memcpy(addr, word, length);

	/* Cleanup after the test */
	pmem2_config_delete(&cfg);
	pmem2_unmap(&map);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_reuse_cfg),
	TEST_CASE(test_reuse_cfg_with_diff_fd),
	TEST_CASE(test_invalid_fd),
	TEST_CASE(test_register_pmem),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_mapping");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
