// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2021, Intel Corporation */

/*
 * pmem_eADR_functions.c -- pmem_eADR_functions unittests
 */
#include "libpmem.h"
#include "out.h"
#include "unittest.h"

/*
 * test_eADR_memmove_256B - do simple memcpy of 256B which should log
 * appropriate memmove function with eADR
 */
static int
test_eADR_memmove_256B(const struct test_case *tc, int argc,
			char *argv[]) {
	if (argc < 1)
		UT_FATAL("usage: test_eADR_forced <path>");

	size_t test_len = 256;
	size_t mapped_len;
	int is_pmem;
	void *const pmemdest = pmem_map_file(argv[0], 0, 0,
		0644, &mapped_len, &is_pmem);

	if (mapped_len <= 0)
		UT_FATAL("mapped_length(%ld) is less or equal 0", mapped_len);

	void *const src = MALLOC(test_len);
	memset(src, 15, test_len);

	pmem_memcpy_nodrain(pmemdest, src, test_len);

	pmem_drain();

	FREE(src);
	if (pmem_unmap(pmemdest, mapped_len) < 0)
		UT_FATAL("unmap error");

	return 1;
}

/*
 * test_eADR_memmove_16MiB - do simple memcpy of 16MiB which should log
 * appropriate memmove function with eADR
 */
static int
test_eADR_memmove_16MiB(const struct test_case *tc, int argc,
			char *argv[]) {
	if (argc < 1)
		UT_FATAL("usage: test_eADR_forced <path>");

	size_t test_len = 16777216;
	size_t mapped_len;
	int is_pmem;
	void *const pmemdest = pmem_map_file(argv[0], 0, 0,
		0644, &mapped_len, &is_pmem);

	if (mapped_len <= 0)
		UT_FATAL("mapped_len(%ld) is less or equal 0", mapped_len);

	if (mapped_len < test_len)
		UT_FATAL("mapped_len(%ld) is less than test_len", mapped_len);

	void *const src = MALLOC(test_len);
	memset(src, 15, test_len);

	pmem_memcpy_nodrain(pmemdest, src, test_len);

	pmem_drain();

	FREE(src);
	if (pmem_unmap(pmemdest, mapped_len) < 0)
		UT_FATAL("unmap error");

	return 1;
}

/*
 * test_eADR_memset_256B - do simple memset of 256B which should log
 * appropriate memset function with eADR
 */
static int
test_eADR_memset_256B(const struct test_case *tc, int argc,
			char *argv[]) {
	if (argc < 1)
		UT_FATAL("usage: test_eADR_forced <path>");

	size_t test_len = 256;
	size_t mapped_len;
	int is_pmem;
	void *const pmemdest = pmem_map_file(argv[0], 0, 0,
		0644, &mapped_len, &is_pmem);

	if (mapped_len <= 0)
		UT_FATAL("mapped_length(%ld) is less or equal 0", mapped_len);

	pmem_memset_nodrain(pmemdest, 1, test_len);

	pmem_drain();
	if (pmem_unmap(pmemdest, mapped_len) < 0)
		UT_FATAL("unmap error");

	return 1;
}

/*
 * test_eADR_memset_16MiB - do simple memsetof 16MiB which should log
 * appropriate memset function with eADR
 */
static int
test_eADR_memset_16MiB(const struct test_case *tc, int argc,
			char *argv[]) {
	if (argc < 1)
		UT_FATAL("usage: test_eADR_forced <path>");

	size_t test_len = 16777216;
	size_t mapped_len;
	int is_pmem;
	void *const pmemdest = pmem_map_file(argv[0], 0, 0,
		0644, &mapped_len, &is_pmem);

	if (mapped_len <= 0)
		UT_FATAL("mapped_len(%ld) is less or equal 0", mapped_len);

	if (mapped_len < test_len)
		UT_FATAL("mapped_len(%ld) is less than test_len", mapped_len);

	pmem_memset_nodrain(pmemdest, 1, test_len);

	pmem_drain();
	if (pmem_unmap(pmemdest, mapped_len) < 0)
		UT_FATAL("unmap error");

	return 1;
}

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_eADR_memmove_256B),
	TEST_CASE(test_eADR_memmove_16MiB),
	TEST_CASE(test_eADR_memset_256B),
	TEST_CASE(test_eADR_memset_16MiB),
};

#define NTESTS (sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char **argv)
{
	START(argc, argv, "pmem_eADR_functions");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
