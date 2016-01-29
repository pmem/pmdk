/*
 * Copyright 2016, Intel Corporation
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
 * libpmempool.h -- definitions of libpmempool entry points
 *
 * See libpmempool(3) for details.
 */

#ifndef LIBPMEMPOOL_H
#define LIBPMEMPOOL_H 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * pool types
 */
enum pmempool_pool_type {
	PMEMPOOL_POOL_TYPE_DETECT,
	PMEMPOOL_POOL_TYPE_LOG,
	PMEMPOOL_POOL_TYPE_BLK,
	PMEMPOOL_POOL_TYPE_OBJ,
	PMEMPOOL_POOL_TYPE_BTT,
};

/*
 * perform repairs
 */
#define PMEMPOOL_CHECK_REPAIR		(1 << 0)
/*
 * emulate repairs
 */
#define PMEMPOOL_CHECK_DRY_RUN		(1 << 1)
/*
 * perform hazardous repairs
 */
#define PMEMPOOL_CHECK_ADVANCED		(1 << 2)
/*
 * do not ask before repairs
 */
#define PMEMPOOL_CHECK_ALWAYS_YES	(1 << 3)
/*
 * generate info statuses
 */
#define PMEMPOOL_CHECK_VERBOSE		(1 << 4)
/*
 * generate string format statuses
 */
#define PMEMPOOL_CHECK_FORMAT_STR	(1 << 5)

/*
 * types of check statuses
 */
enum pmempool_check_msg_type {
	PMEMPOOL_CHECK_MSG_TYPE_INFO,
	PMEMPOOL_CHECK_MSG_TYPE_ERROR,
	PMEMPOOL_CHECK_MSG_TYPE_QUESTION,
};

/*
 * check context arguments
 */
struct pmempool_check_args {
	const char *path;
	const char *backup_path;
	enum pmempool_pool_type pool_type;
	int flags;
};

/*
 * check result types
 */
enum pmempool_check_result {
	PMEMPOOL_CHECK_RESULT_CONSISTENT,
	PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT,
	PMEMPOOL_CHECK_RESULT_REPAIRED,
	PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR,
	PMEMPOOL_CHECK_RESULT_ERROR,
};

/*
 * check status
 */
struct pmempool_check_status {
	enum pmempool_check_msg_type type;
	struct {
		const char *msg;
		const char *answer;
	} str;
};

/*
 * check context
 */
typedef struct pmempool_check PMEMpoolcheck;

/*
 * initialize a check context
 */
PMEMpoolcheck *
pmempool_check_init(struct pmempool_check_args *args, size_t args_size);

/*
 * start / resume the check
 */
struct pmempool_check_status *pmempool_check(PMEMpoolcheck *ppc);

/*
 * finalize the check and get the result
 */
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck *ppc);

/*
 * PMEMPOOL_MAJOR_VERSION and PMEMPOOL_MINOR_VERSION provide the current version
 * of the libpmempool API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmempool_check_version().
 */
#define PMEMPOOL_MAJOR_VERSION 1
#define PMEMPOOL_MINOR_VERSION 0
const char *pmempool_check_version(unsigned major_required,
	unsigned minor_required);

/*
 * get last error message
 */
const char *pmempool_errormsg(void);

#ifdef __cplusplus
}
#endif
#endif	/* libpmempool.h */
