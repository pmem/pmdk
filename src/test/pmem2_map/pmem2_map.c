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
 * pmem2_map.c -- pmem2_map unittests
 */

#include "config.h"
#include "map.h"
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2_utils.h"

#define KILOBYTE (1 << 10)
#define MEGABYTE (1 << 20)

static size_t
get_size(char *file)
{
	os_stat_t stbuf;
	STAT(file, &stbuf);

	return (size_t)stbuf.st_size;
}

static void
prepare_config(struct pmem2_config **cfg, char *file, size_t length,
		size_t addr, int access)
{
	int fd = OPEN(file, access);

	int ret = (pmem2_config_new(cfg));
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	(*cfg)->offset = addr;
	(*cfg)->length = length;
#ifdef WIN32
	(*cfg)->handle = (HANDLE)_get_osfhandle(fd);
#else
	(*cfg)->fd = fd;
#endif
}

static void
close_file(struct pmem2_config **cfg)
{
#ifdef WIN32
	CloseHandle((*cfg)->handle);
#else
	CLOSE((*cfg)->fd);
#endif
}

static void
cleanup(struct pmem2_config **cfg)
{
	close_file(cfg);
	pmem2_config_delete(cfg);
}

/*
 * test_map_rdrw_file - test mapping file in read/write mode
 */
static int
test_map_rdrw_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	size_t size = get_size(file);
	prepare_config(&cfg, file, size, 0, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size);

	cleanup(&cfg);

	return 1;
}

/*
 * test_map_rdonly_file - test mapping file in read mode
 */
static int
test_map_rdonly_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	size_t size = get_size(file);
	prepare_config(&cfg, file, size, 0, O_RDONLY);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size);

	cleanup(&cfg);

	return 1;
}

/*
 * test_map_accmode_file - test mapping file in access mode (linux only)
 */
#ifndef WIN32
static int
test_map_accmode_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	size_t size = get_size(file);
	prepare_config(&cfg, file, size, 0, O_ACCMODE);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_FAILED);

	cleanup(&cfg);

	return 1;
}
#else
static int
test_map_accmode_file(const struct test_case *tc, int argc, char *argv[])
{
	return 0;
}
#endif

/*
 * test_map_valid_range_map - test mapping file valid memory range
 */
static int
test_map_valid_range_map(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	size_t size = get_size(file);
	struct pmem2_map *map;
	int ret = 0;

	prepare_config(&cfg, file, 0, 0, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size);
	pmem2_unmap(&map);
	cleanup(&cfg);

	prepare_config(&cfg, file, size, 0, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size);
	pmem2_unmap(&map);
	cleanup(&cfg);

	prepare_config(&cfg, file, size / 2, 0, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size / 2);
	pmem2_unmap(&map);
	cleanup(&cfg);

	prepare_config(&cfg, file, size / 2, 2 * MEGABYTE, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size / 2);
	pmem2_unmap(&map);
	cleanup(&cfg);

	return 1;
}

/*
 * test_map_invalid_range_map - test mapping file beyond valid memory range
 */
static int
test_map_invalid_range_map(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	size_t size = get_size(file);
	struct pmem2_map *map;
	int ret = 0;

	prepare_config(&cfg, file, size + 1, 0, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	cleanup(&cfg);

	prepare_config(&cfg, file, size / 2, 10 * MEGABYTE, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	cleanup(&cfg);

	prepare_config(&cfg, file, 0, 20 * MEGABYTE, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	cleanup(&cfg);

	return 1;
}

/*
 * test_map_invalid_args - test try to mapping file using invalid arguments
 */
static int
test_map_invalid_args(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	size_t length = get_size(file) / 2;
	struct pmem2_map *map;
	int ret = 0;

	/* invalid alignment in addres */
	prepare_config(&cfg, file, length, KILOBYTE, O_RDWR);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_FAILED);
	cleanup(&cfg);

	/* invalid file descriptor */
	prepare_config(&cfg, file, length, KILOBYTE, O_RDWR);
	close_file(&cfg);
	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_UNKNOWN_FILETYPE);
	pmem2_config_delete(&cfg);

	/* empty config */
	ret = pmem2_config_new(&cfg);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_UNKNOWN_FILETYPE);
	pmem2_config_delete(&cfg);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_map_rdrw_file),
	TEST_CASE(test_map_rdonly_file),
	TEST_CASE(test_map_accmode_file),
	TEST_CASE(test_map_valid_range_map),
	TEST_CASE(test_map_invalid_range_map),
	TEST_CASE(test_map_invalid_args),
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
