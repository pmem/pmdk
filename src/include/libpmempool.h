/*
 * Copyright 2016-2017, Intel Corporation
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

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef NVML_UTF8_API
#define pmempool_check_status pmempool_check_statusW
#define pmempool_check_args pmempool_check_argsW

#define pmempool_check_init pmempool_check_initW
#define pmempool_check pmempool_checkW
#define pmempool_sync pmempool_syncW
#define pmempool_transform pmempool_transformW
#define pmempool_rm pmempool_rmW
#define pmempool_check_version pmempool_check_versionW
#define pmempool_errormsg pmempool_errormsgW
#else
#define pmempool_check_status pmempool_check_statusU
#define pmempool_check_args pmempool_check_argsU

#define pmempool_check_init pmempool_check_initU
#define pmempool_check pmempool_checkU
#define pmempool_sync pmempool_syncU
#define pmempool_transform pmempool_transformU
#define pmempool_rm pmempool_rmU
#define pmempool_check_version pmempool_check_versionU
#define pmempool_errormsg pmempool_errormsgU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <limits.h>


/* COMMON FLAGS */

/*
 * do not apply changes, only check if operation is viable
 */
#define PMEMPOOL_DRY_RUN (1 << 1)


/* PMEMPOOL CHECK */

/*
 * pool types
 */
enum pmempool_pool_type {
	PMEMPOOL_POOL_TYPE_DETECT,
	PMEMPOOL_POOL_TYPE_LOG,
	PMEMPOOL_POOL_TYPE_BLK,
	PMEMPOOL_POOL_TYPE_OBJ,
	PMEMPOOL_POOL_TYPE_BTT,
	PMEMPOOL_POOL_TYPE_CTO,
};

/*
 * perform repairs
 */
#define PMEMPOOL_CHECK_REPAIR		(1 << 0)
/*
 * emulate repairs
 */
#define PMEMPOOL_CHECK_DRY_RUN PMEMPOOL_DRY_RUN
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
 * check context
 */
typedef struct pmempool_check_ctx PMEMpoolcheck;

/*
 * finalize the check and get the result
 */
enum pmempool_check_result pmempool_check_end(PMEMpoolcheck *ppc);

/* PMEMPOOL RM */

#define PMEMPOOL_RM_FORCE		(1 << 0) /* ignore any errors */
#define PMEMPOOL_RM_POOLSET_LOCAL	(1 << 1) /* remove local poolsets */
#define PMEMPOOL_RM_POOLSET_REMOTE	(1 << 2) /* remove remote poolsets */

/*
 * PMEMPOOL_MAJOR_VERSION and PMEMPOOL_MINOR_VERSION provide the current version
 * of the libpmempool API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmempool_check_version().
 */
#define PMEMPOOL_MAJOR_VERSION 1
#define PMEMPOOL_MINOR_VERSION 2

/*
 * check status
 */
struct pmempool_check_statusU {
	enum pmempool_check_msg_type type;
	struct {
		const char *msg;
		const char *answer;
	} str;
};

#ifndef _WIN32
#define pmempool_check_status pmempool_check_statusU
#else
struct pmempool_check_statusW {
	enum pmempool_check_msg_type type;
	struct {
		const wchar_t *msg;
		const wchar_t *answer;
	} str;
};
#endif

/*
 * check context arguments
 */
struct pmempool_check_argsU {
	const char *path;
	const char *backup_path;
	enum pmempool_pool_type pool_type;
	int flags;
};

#ifndef _WIN32
#define pmempool_check_args pmempool_check_argsU
#else
struct pmempool_check_argsW {
	const wchar_t *path;
	const wchar_t *backup_path;
	enum pmempool_pool_type pool_type;
	int flags;
};
#endif

/*
 * initialize a check context
 */
#ifndef _WIN32
PMEMpoolcheck *
pmempool_check_init(struct pmempool_check_args *args, size_t args_size);
#else
PMEMpoolcheck *
pmempool_check_initU(struct pmempool_check_argsU *args, size_t args_size);
PMEMpoolcheck *
pmempool_check_initW(struct pmempool_check_argsW *args, size_t args_size);
#endif

/*
 * start / resume the check
 */
#ifndef _WIN32
struct pmempool_check_status *pmempool_check(PMEMpoolcheck *ppc);
#else
struct pmempool_check_statusU *pmempool_checkU(PMEMpoolcheck *ppc);
struct pmempool_check_statusW *pmempool_checkW(PMEMpoolcheck *ppc);
#endif

/*
 * LIBPMEMPOOL SYNC & TRANSFORM
 */

/*
 * Synchronize data between replicas within a poolset.
 *
 * EXPERIMENTAL
 */
#ifndef _WIN32
int pmempool_sync(const char *poolset_file, unsigned flags);
#else
int pmempool_syncU(const char *poolset_file, unsigned flags);
int pmempool_syncW(const wchar_t *poolset_file, unsigned flags);
#endif

/*
 * Modify internal structure of a poolset.
 *
 * EXPERIMENTAL
 */
#ifndef _WIN32
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags);
#else
int pmempool_transformU(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags);
int pmempool_transformW(const wchar_t *poolset_file_src,
	const wchar_t *poolset_file_dst, unsigned flags);
#endif

/* PMEMPOOL RM */
#ifndef _WIN32
int pmempool_rm(const char *path, int flags);
#else
int pmempool_rmU(const char *path, int flags);
int pmempool_rmW(const wchar_t *path, int flags);
#endif

#ifndef _WIN32
const char *pmempool_check_version(unsigned major_required,
	unsigned minor_required);
#else
const char *pmempool_check_versionU(unsigned major_required,
	unsigned minor_required);
const wchar_t *pmempool_check_versionW(unsigned major_required,
	unsigned minor_required);
#endif

#ifndef _WIN32
const char *pmempool_errormsg(void);
#else
const char *pmempool_errormsgU(void);
const wchar_t *pmempool_errormsgW(void);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* libpmempool.h */
