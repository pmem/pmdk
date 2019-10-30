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
 * pmem2_mapping.c -- pmem2_map integration tests
 */

#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"

//#define KILOBYTE (1 << 10)
//#define MEGABYTE (1 << 20)

static size_t
get_size(char *file)
{
	os_stat_t stbuf;
	STAT(file, &stbuf);

	return (size_t)stbuf.st_size;
}

static void
pmem2_prepare_config(struct pmem2_config **cfg, char *file, int access)
{
	int fd = OPEN(file, access);

	int ret = (pmem2_config_new(cfg));
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	ret = pmem2_config_set_fd(*cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
}

static int
test_reuse_cfg(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	struct pmem2_config *cfg;
	char *file = argv[0];
	size_t size = get_size(file);

	pmem2_prepare_config(&cfg, file, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(pmem2_map_get_size(map), size);

	struct pmem2_map *map2;
	ret = pmem2_map(cfg, &map2);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(pmem2_map_get_size(map2), size);

	pmem2_config_delete(&cfg);
	pmem2_unmap(&map);
	pmem2_unmap(&map2);

	return 1;
}

static int
test_reuse_cfg_with_diff_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s test_case file file2", argv[0]);

	struct pmem2_config *cfg;
	char *file = argv[0];
	size_t size = get_size(file);
	char *file2 = argv[1];
	size_t size2 = get_size(file2);

	pmem2_prepare_config(&cfg, file, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(pmem2_map_get_size(map), size);

	int fd = OPEN(file2, O_RDWR);
	struct pmem2_map *map2;
	ret = pmem2_config_set_fd(cfg, fd);
	ret = pmem2_map(cfg, &map2);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(pmem2_map_get_size(map2), size2);

	ret = pmem2_config_set_fd(cfg, -1);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_ARG);

	pmem2_config_delete(&cfg);
	pmem2_unmap(&map);
	pmem2_unmap(&map2);

	return 1;
}

static int
test_invalid_fd(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	struct pmem2_config *cfg;
	char *file = argv[0];
	size_t size = get_size(file);

	pmem2_prepare_config(&cfg, file, O_WRONLY);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(pmem2_map_get_size(map), size);

	pmem2_config_delete(&cfg);

	return 1;
}

static int
test_register_pmem(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	struct pmem2_config *cfg;
	char *file = argv[0];
	size_t size = get_size(file);
	char *word = "XXXXXXXX";
	pmem2_prepare_config(&cfg, file, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(pmem2_map_get_size(map), size);
	void *addr = pmem2_map_get_address(map);
	size_t s = sizeof(word);
	memcpy(addr, word, s);

	pmem2_config_delete(&cfg);

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
	START(argc, argv, "pmem2_map");
	out_init("pmem2_map", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();
	DONE(NULL);
}
