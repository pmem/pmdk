/*
 * Copyright 2017, Intel Corporation
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
};

/* all values to test */
const enum rpmem_persist_method pms[] =
		{RPMEM_PM_GPSPM, RPMEM_PM_APM, MAX_RPMEM_PM};
const int is_pmems[] = {0, 1};

/* expected results */
const struct result exp_results[3][2] = {
		{
			/* GPSPM and is_pmem == false */
			{0, RPMEM_PM_GPSPM, pmem_msync},
			/* GPSPM and is_pmem == true */
			{0, RPMEM_PM_GPSPM, rpmemd_pmem_persist}
		}, {
			/* APM and is_pmem == false */
			{0, RPMEM_PM_GPSPM, pmem_msync},
			/* APM and is_pmem == true */
			{0, RPMEM_PM_APM, rpmemd_pmem_persist}
		}, {
			/* persistency method outside of the range */
			{1, 0, 0},
			{1, 0, 0}
		}
};

static void
test_apply_pm_policy(struct result *result, int is_pmem)
{
	if (rpmemd_apply_pm_policy(&result->persist_method, &result->persist,
			is_pmem)) {
		goto err;
	}

	result->ret = 0;
	return;
err:
	result->ret = 1;
}

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmemd_util");

	int ret = rpmemd_log_init("rpmemd_log", NULL, 0);
	UT_ASSERTeq(ret, 0);

	struct result result;
	const struct result *exp_result;

	for (int pm_ind = 0; pm_ind < ARRAY_SIZE(pms); ++pm_ind) {
		for (int is_pmem_ind = 0; is_pmem_ind < ARRAY_SIZE(is_pmems);
				++is_pmem_ind) {

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

	DONE(NULL);
	return 0;
}
