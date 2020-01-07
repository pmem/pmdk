// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * pmempool.h -- internal definitions for libpmempool
 */

#ifndef PMEMPOOL_H
#define PMEMPOOL_H

#ifdef __cplusplus
extern "C" {
#endif

#define PMEMPOOL_LOG_PREFIX "libpmempool"
#define PMEMPOOL_LOG_LEVEL_VAR "PMEMPOOL_LOG_LEVEL"
#define PMEMPOOL_LOG_FILE_VAR "PMEMPOOL_LOG_FILE"

enum check_result {
	CHECK_RESULT_CONSISTENT,
	CHECK_RESULT_NOT_CONSISTENT,
	CHECK_RESULT_ASK_QUESTIONS,
	CHECK_RESULT_PROCESS_ANSWERS,
	CHECK_RESULT_REPAIRED,
	CHECK_RESULT_CANNOT_REPAIR,
	CHECK_RESULT_ERROR,
	CHECK_RESULT_INTERNAL_ERROR
};

/*
 * pmempool_check_ctx -- context and arguments for check command
 */
struct pmempool_check_ctx {
	struct pmempool_check_args args;
	char *path;
	char *backup_path;

	struct check_data *data;
	struct pool_data *pool;
	enum check_result result;
	unsigned sync_required;
};

#ifdef __cplusplus
}
#endif

#endif
