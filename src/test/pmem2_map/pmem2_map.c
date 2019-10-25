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
 * prepare_config -- allocate pmem2_config and fill it
 */
static void
prepare_config(struct pmem2_config *cfg, char *file, size_t length,
		size_t addr, int access)
{
	int fd = OPEN(file, access);

	config_init(cfg);
	cfg->offset = addr;
	cfg->length = length;
#ifdef WIN32
	cfg->handle = (HANDLE)_get_osfhandle(fd);
#else
	cfg->fd = fd;
#endif
}

/*
 * config_close_file -- close the pmem2_config file handle
 */
static void
config_close_file(struct pmem2_config *cfg)
{
#ifdef WIN32
	CloseHandle(cfg->handle);
#else
	CLOSE(cfg->fd);
#endif
}

/*
 * config_cleanup -- clean up the pmem2_config resources
 * - close the file handle
 * - release the pmem2_config memory
 */
static void
config_cleanup(struct pmem2_config *cfg)
{
	config_close_file(cfg);
	config_init(cfg);
}

/*
 * test_map_rdrw_file - map a O_RDWR file
 */
static int
test_map_rdrw_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_rdrw_file <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	prepare_config(&cfg, file, 0, 0, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(&cfg, &map);

	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	config_cleanup(&cfg);

	return 1;
}

/*
 * test_map_rdonly_file - map a O_RDONLY file
 */
static int
test_map_rdonly_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_rdonly_file <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	prepare_config(&cfg, file, 0, 0, O_RDONLY);

	struct pmem2_map *map;
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	config_cleanup(&cfg);

	return 1;
}

/*
 * test_map_wronly_file - map a O_WRONLY file
 */
static int
test_map_wronly_file(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_wronly_file <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	prepare_config(&cfg, file, 0, 0, O_WRONLY);

	struct pmem2_map *map;
	int ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, -EACCES);

	config_cleanup(&cfg);

	return 1;
}

/*
 * test_map_valid_ranges - map valid memory ranges
 */
static int
test_map_valid_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_valid_ranges <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	size_t size = get_size(file);
	struct pmem2_map *map;
	int ret = 0;

	/* the config WITHOUT provided length allows mapping the whole file */
	prepare_config(&cfg, file, 0, 0, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size);
	pmem2_unmap(&map);
	config_cleanup(&cfg);

	/* the config WITH provided length allows mapping the whole file */
	prepare_config(&cfg, file, size, 0, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size);
	pmem2_unmap(&map);
	config_cleanup(&cfg);

	/* the config with provided length different than the file length */
	prepare_config(&cfg, file, size / 2, 0, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size / 2);
	pmem2_unmap(&map);
	config_cleanup(&cfg);

	/* verify the config with provided length and a valid offset */
	prepare_config(&cfg, file, size / 2, 2 * MEGABYTE, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);
	UT_ASSERTeq(map->length, size / 2);
	pmem2_unmap(&map);
	config_cleanup(&cfg);

	return 1;
}

/*
 * test_map_invalid_ranges - map invalid memory ranges
 */
static int
test_map_invalid_ranges(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_invalid_ranges <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	size_t size = get_size(file);
	struct pmem2_map *map;
	int ret = 0;

	/* the mapping size (unaligned) > the file size */
	prepare_config(&cfg, file, size + 1, 0, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	config_cleanup(&cfg);

	/* the mapping + the offset > the file size */
	UT_ASSERT(size / 2 < 10 * MEGABYTE);
	prepare_config(&cfg, file, size / 2, 10 * MEGABYTE, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	config_cleanup(&cfg);

	/* the mapping size (aligned) > the file size */
	UT_ASSERT(size < 20 * MEGABYTE);
	prepare_config(&cfg, file, 0, 20 * MEGABYTE, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_MAP_RANGE);
	config_cleanup(&cfg);

	return 1;
}

/*
 * test_map_invalid_args - map using miscellaneous invalid arguments
 */
static int
test_map_invalid_args(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_map_invalid_args <file>", argv[0]);

	char *file = argv[0];
	struct pmem2_config cfg;
	size_t length = get_size(file) / 2;
	struct pmem2_map *map;
	int ret = 0;

	/* invalid alignment in the offset */
	prepare_config(&cfg, file, length, KILOBYTE, O_RDWR);
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, -EINVAL);
	config_cleanup(&cfg);

	/* the invalid file descriptor */
	prepare_config(&cfg, file, length, 0, O_RDWR);
	config_close_file(&cfg);
	ret = pmem2_map(&cfg, &map);
#ifdef WIN32
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_HANDLE);
#else
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_UNKNOWN_FILETYPE);
#endif
	config_init(&cfg);

	/* empty config */
	/* config already zero-initialized */
	ret = pmem2_map(&cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_HANDLE);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_map_rdrw_file),
	TEST_CASE(test_map_rdonly_file),
	TEST_CASE(test_map_wronly_file),
	TEST_CASE(test_map_valid_ranges),
	TEST_CASE(test_map_invalid_ranges),
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
