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
 * pmem2_granularity.c -- unit test for graunlarity functionality
 */

#include <string.h>
#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "ut_pmem2_config.h"
#define PMEM2_GRANULARITY_INVALID 99

static void
test_prepare(struct pmem2_config **cfg, int fd)
{
	int ret = pmem2_config_new(cfg);
	UT_PMEM2_EXPECT_RETURN(ret, 0);

	ret = pmem2_config_set_fd(*cfg, fd);
	UT_PMEM2_EXPECT_RETURN(ret, 0);
}

static void
test_finish(struct pmem2_config **cfg)
{
	pmem2_config_delete(cfg);
}

/*
 * test_granularity_inval -- pass invalid granularity value
 */
static int
test_granularity_inval(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: test_granularity_inval <file>");

	char *file = argv[0];
	int fd = OPEN(file, O_RDWR);
	UT_ASSERTne(fd, -1);

	struct pmem2_config *cfg;
	test_prepare(&cfg, fd);

	enum pmem2_granularity g = PMEM2_GRANULARITY_INVALID;
	int ret = pmem2_config_set_required_store_granularity(cfg, g);
	UT_PMEM2_EXPECT_RETURN(ret, PMEM2_E_INVALID_ARG);

	test_finish(&cfg);
	CLOSE(fd);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_granularity_inval),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "pmem2_granularity");
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	DONE(NULL);
}
