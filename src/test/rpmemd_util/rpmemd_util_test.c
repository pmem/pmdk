// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017-2018, Intel Corporation */

/*
 * rpmemd_util_test.c -- unit tests for rpmemd_util module
 */

#include "unittest.h"
#include "rpmem_common.h"
#include "rpmemd_log.h"
#include "rpmemd_util.h"
#include "util.h"

/* structure to store results */
struct result {
	int ret;
	enum rpmem_persist_method persist_method;
	int (*persist)(const void *addr, size_t len);
	void *(*memcpy_persist)(void *pmemdest, const void *src, size_t len);
};

/* all values to test */
static const enum rpmem_persist_method pms[] =
		{RPMEM_PM_GPSPM, RPMEM_PM_APM, MAX_RPMEM_PM};
static const int is_pmems[] = {0, 1};

enum mode {
	MODE_VALID,
	MODE_INVALID,
	MODE_MAX
};

static const int ranges[2][2][2] = {
	[MODE_VALID] = {
		{0, ARRAY_SIZE(pms) - 1},
		{0, ARRAY_SIZE(is_pmems)}
	},
	[MODE_INVALID] = {
		{ARRAY_SIZE(pms) - 1,	ARRAY_SIZE(pms)},
		{0,			ARRAY_SIZE(is_pmems)}
	}
};

/* expected results */
static const struct result exp_results[3][2] = {
		{
			/* GPSPM and is_pmem == false */
			{0, RPMEM_PM_GPSPM, pmem_msync, memcpy},
			/* GPSPM and is_pmem == true */
			{0, RPMEM_PM_GPSPM, rpmemd_pmem_persist,
				pmem_memcpy_persist}
		}, {
			/* APM and is_pmem == false */
			{0, RPMEM_PM_GPSPM, pmem_msync, memcpy},
			/* APM and is_pmem == true */
			{0, RPMEM_PM_APM, rpmemd_flush_fatal,
				pmem_memcpy_persist}
		}, {
			/* persistency method outside of the range */
			{1, 0, 0, 0},
			{1, 0, 0, 0}
		}
};

static void
test_apply_pm_policy(struct result *result, int is_pmem)
{
	if (rpmemd_apply_pm_policy(&result->persist_method, &result->persist,
				&result->memcpy_persist, is_pmem)) {
		goto err;
	}

	result->ret = 0;
	return;
err:
	result->ret = 1;
}

#define USAGE() do {\
	UT_ERR("usage: %s valid|invalid", argv[0]);\
} while (0)

static void
test(const int pm_range[2], const int is_pmem_range[2])
{
	rpmemd_log_level = RPD_LOG_NOTICE;
	int ret = rpmemd_log_init("rpmemd_log", NULL, 0);
	UT_ASSERTeq(ret, 0);

	struct result result;
	const struct result *exp_result;

	for (int pm_ind = pm_range[0]; pm_ind < pm_range[1]; ++pm_ind) {
		for (int is_pmem_ind = is_pmem_range[0];
				is_pmem_ind < is_pmem_range[1]; ++is_pmem_ind) {

			result.persist_method = pms[pm_ind];

			exp_result = &exp_results[pm_ind][is_pmem_ind];

			test_apply_pm_policy(&result, is_pmems[is_pmem_ind]);

			UT_ASSERTeq(result.ret, exp_result->ret);

			if (exp_result->ret == 0) {
				UT_ASSERTeq(result.persist_method,
						exp_result->persist_method);
				UT_ASSERTeq(result.persist,
						exp_result->persist);
			}
		}
	}

	rpmemd_log_close();
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmemd_util");

	if (argc < 2) {
		USAGE();
		return 1;
	}

	const char *mode_str = argv[1];
	enum mode mode = MODE_MAX;

	if (strcmp(mode_str, "valid") == 0) {
		mode = MODE_VALID;
	} else if (strcmp(mode_str, "invalid") == 0) {
		mode = MODE_INVALID;
	} else {
		USAGE();
		return 1;
	}
	UT_ASSERTne(mode, MODE_MAX);

	test(ranges[mode][0], ranges[mode][1]);

	DONE(NULL);
}
