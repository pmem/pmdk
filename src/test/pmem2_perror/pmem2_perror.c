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
 * save_file_to_string - reads a file and returns it as a string
 */
static char *
save_file_to_string(FILE *fp, char *file_name)
{
	char *buffer = NULL;
	long file_len;

	fp = os_fopen(file_name, "r");
	if (!fp) {
		ERR("!os_fopen");
		return NULL;
	}

	if (fseek(fp, 0, SEEK_END)) {
		ERR("!fseek");
		return NULL;
	}

	file_len = ftell(fp);
	if (file_len < 0) {
		ERR("!ftell");
		return NULL;
	}

	if (fseek(fp, 0, SEEK_SET)) {
		ERR("!fseek");
		return NULL;
	}

	buffer = malloc((size_t)file_len);
	if (!buffer) {
		ERR("!malloc");
		return NULL;
	}

	if (!fread(buffer, 1, (size_t)file_len, fp)) {
		ERR("!fread");
		free(buffer);
		return NULL;
	}

	if (fclose(fp)) {
		ERR("!fclose");
		free(buffer);
		return NULL;
	}

	return buffer;
}

/*
 * test_simple_check - simply check print message
 */
static int
test_simple_check(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 2)
		UT_FATAL("usage: test_simple_check <file1> <file2>");

	char *file1 = argv[0];
	char *file2 = argv[1];

	/* "randomly" chosen errno value */
	errno = EINVAL;
	ERR("!");

	FILE *fp1;
	fp1 = freopen(file1, "r+", stderr);
	pmem2_perror("test");
	fclose(fp1);

	FILE *fp2;
	fp2 = freopen(file2, "r+", stderr);
	perror("test");
	fclose(fp2);

	char *pmem2_perror_message = save_file_to_string(fp1, file1);
	UT_ASSERTne(NULL, pmem2_perror_message);

	char *perror_message = save_file_to_string(fp2, file2);
	if (!perror_message)
		free(pmem2_perror_message);
	UT_ASSERTne(NULL, perror_message);

	int ret = strcmp(pmem2_perror_message, perror_message);
	UT_ASSERTeq(0, ret);

	free(pmem2_perror_message);
	free(perror_message);

	return 2;
}

/*
 * test_format_check - check print message with used ellipsis operator
 */
static int
test_format_check(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: test_format_check <file>");

	char *file = argv[0];

	/* "randomly" chosen errno value */
	errno = EISDIR;
	ERR("!");

	/*
	 * expected value of print, when errno == EISDIR and pmem2_perror
	 * takes arg == ("test %d", 123)
	 */
	const char *expected_message = "test 123: Is a directory\n";

	FILE *fp1;
	fp1 = freopen(file, "r+", stderr);
	pmem2_perror("test %d", 123);
	fclose(fp1);

	char *pmem2_perror_message = save_file_to_string(fp1, file);
	UT_ASSERTne(NULL, pmem2_perror_message);

	int ret = strcmp(expected_message, pmem2_perror_message);
	UT_ASSERTeq(0, ret);

	free(pmem2_perror_message);

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_simple_check),
	TEST_CASE(test_format_check),
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
