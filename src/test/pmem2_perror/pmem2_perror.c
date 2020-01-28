/*
 * Copyright 2020, Intel Corporation
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
 * pmem2_perror.c -- pmem2_perror unittests
 */

#include "unittest.h"
#include "ut_pmem2_utils.h"
#include "out.h"

/*
 * test_simple_check_fail - simply check print message, fail expected
 */
static int
test_simple_check_fail(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen errno value */
	errno = EINVAL;
	ERR("!");

	/*
	 * expected value of print, when errno == EINVAL and pmem2_perror
	 * takes arg == "test"
	 */
	const char *expected = "test: Invalid argument\n";

	/* let's change stderr's buffer */
	char buf[BUFSIZ];
	setbuf(stderr, buf);

	/* let's simulate the fail, write "XXXX" instead of "test" */
	pmem2_perror("XXXX");
	int ret = strcmp(expected, buf);
	UT_ASSERTne(0, ret);

	return 0;
}

/*
 * test_simple_check_pass - simply check print message, pass expected
 */
static int
test_simple_check_pass(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen errno value */
	errno = EINVAL;
	ERR("!");

	/*
	 * expected value of print, when errno == EINVAL and pmem2_perror
	 * takes arg == "test"
	 */
	const char *expected = "test: Invalid argument\n";

	/* let's change stderr's buffer */
	char buf[BUFSIZ];
	setbuf(stderr, buf);

	/* let's pass */
	pmem2_perror("test");
	int ret = strcmp(expected, buf);
	UT_ASSERTeq(0, ret);

	return 0;
}

/*
 * test_format_check_fail - check print message with used ellipsis
 * operator, fail expected
 */
static int
test_format_check_fail(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen errno value */
	errno = EISDIR;
	ERR("!");

	/*
	 * expected value of print, when errno == EINVAL and pmem2_perror
	 * takes arg == ("test %d", 123)
	 */
	const char *expected = "test 123: Is a directory\n";

	/* let's change stderr's buffer */
	char buf[BUFSIZ];
	setbuf(stderr, buf);

	/* let's simulate the fail, write "XXXX" instead of ("test %d", 123) */
	pmem2_perror("XXXX");
	int ret = strcmp(expected, buf);
	UT_ASSERTne(0, ret);

	return 0;
}

/*
 * test_format_check_pass - check print message with used ellipsis
 * operator, pass expected
 */
static int
test_format_check_pass(const struct test_case *tc, int argc, char *argv[])
{
	/* "randomly" chosen errno value */
	errno = EISDIR;
	ERR("!");

	/*
	 * expected value of print, when errno == EINVAL and pmem2_perror
	 * takes arg == ("test %d", 123)
	 */
	const char *expected = "test 123: Is a directory\n";

	/* let's change stderr's buffer */
	char buf[BUFSIZ];
	setbuf(stderr, buf);

	/* let's pass */
	pmem2_perror("test %d", 123);
	int ret = strcmp(expected, buf);
	UT_ASSERTeq(0, ret);

	return 0;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_simple_check_fail),
	TEST_CASE(test_simple_check_pass),
	TEST_CASE(test_format_check_fail),
	TEST_CASE(test_format_check_pass),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem2_perror");

	util_init();
	out_init("pmem2_perror", "TEST_LOG_LEVEL", "TEST_LOG_FILE", 0, 0);
	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);
	out_fini();

	DONE(NULL);
}
