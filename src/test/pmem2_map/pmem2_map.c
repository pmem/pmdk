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
#include "out.h"
#include "pmem2.h"
#include "unittest.h"
#include "ut_pmem2_config.h"
#include "ut_pmem2_utils.h"

static size_t
get_size(char *file)
{
	os_stat_t stbuf;
	STAT(file, &stbuf);

	return (size_t)stbuf.st_size;
}

static void
close_file(struct pmem2_config *cfg)
{
#ifdef WIN32
	CloseHandle(cfg->handle);
#else
	CLOSE(cfg->fd);
#endif
}

static void
test_prepare_config(struct pmem2_config **cfg, char *file, int access)
{
	size_t size = get_size(file);
	int fd = OPEN(file, access);

	int ret = (pmem2_config_new(cfg));
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	(*cfg)->offset = 0;
	(*cfg)->length = size;
#ifdef WIN32
	(*cfg)->handle = (HANDLE)_get_osfhandle(fd);
#else
	(*cfg)->fd = fd;
#endif
}

/*
 * test_map_get_size -- check pmem2_map_get_address func
 */
static int
test_map_get_address(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	test_prepare_config(&cfg, file, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	void *addr = pmem2_map_get_address(map);
	UT_ASSERTeq(addr, map->addr);

	close_file(cfg);

	return 1;
}

/*
 * test_map_get_size -- check pmem2_map_get_size func
 */
static int
test_map_get_size(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s test_case file", argv[0]);

	char *file = argv[0];
	struct pmem2_config *cfg;
	test_prepare_config(&cfg, file, O_RDWR);

	struct pmem2_map *map;
	int ret = pmem2_map(cfg, &map);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_OK);

	size_t size = pmem2_map_get_size(map);
	UT_ASSERTeq(size, get_size(file));

	close_file(cfg);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_map_get_address),
	TEST_CASE(test_map_get_size),
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
