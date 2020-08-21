/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

/*
 * libpmempool.h -- definitions of libpmempool entry points
 *
 * See libpmempool(7) for details.
 */

#ifndef LIBPMEMPOOL_H
#define LIBPMEMPOOL_H 1

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef _WIN32
#include <pmemcompat.h>

#ifndef PMDK_UTF8_API
#define pmempool_check_status pmempool_check_statusW
#define pmempool_check_args pmempool_check_argsW

#define pmempool_check_init pmempool_check_initW
#define pmempool_check pmempool_checkW
#define pmempool_sync pmempool_syncW
#define pmempool_transform pmempool_transformW
#define pmempool_rm pmempool_rmW
#define pmempool_check_version pmempool_check_versionW
#define pmempool_errormsg pmempool_errormsgW
#define pmempool_feature_enable pmempool_feature_enableW
#define pmempool_feature_disable pmempool_feature_disableW
#define pmempool_feature_query pmempool_feature_queryW
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
#define pmempool_feature_enable pmempool_feature_enableU
#define pmempool_feature_disable pmempool_feature_disableU
#define pmempool_feature_query pmempool_feature_queryU
#endif

#endif

#ifdef __cplusplus
extern "C" {
#endif

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
	PMEMPOOL_POOL_TYPE_RESERVED1, /* used to be cto */
};

/*
 * perform repairs
 */
#define PMEMPOOL_CHECK_REPAIR		(1U << 0)
/*
 * emulate repairs
 */
#define PMEMPOOL_CHECK_DRY_RUN		(1U << 1)
/*
 * perform hazardous repairs
 */
#define PMEMPOOL_CHECK_ADVANCED		(1U << 2)
/*
 * do not ask before repairs
 */
#define PMEMPOOL_CHECK_ALWAYS_YES	(1U << 3)
/*
 * generate info statuses
 */
#define PMEMPOOL_CHECK_VERBOSE		(1U << 4)
/*
 * generate string format statuses
 */
#define PMEMPOOL_CHECK_FORMAT_STR	(1U << 5)

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
	PMEMPOOL_CHECK_RESULT_SYNC_REQ,
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

#define PMEMPOOL_RM_FORCE		(1U << 0) /* ignore any errors */
#define PMEMPOOL_RM_POOLSET_LOCAL	(1U << 1) /* remove local poolsets */
#define PMEMPOOL_RM_POOLSET_REMOTE	(1U << 2) /* remove remote poolsets */

/*
 * LIBPMEMPOOL SYNC
 */

/*
 * fix bad blocks - it requires creating or reading special recovery files
 */
#define PMEMPOOL_SYNC_FIX_BAD_BLOCKS	(1U << 0)
/*
 * do not apply changes, only check if operation is viable
 */
#define PMEMPOOL_SYNC_DRY_RUN		(1U << 1)

/*
 * LIBPMEMPOOL TRANSFORM
 */

/*
 * do not apply changes, only check if operation is viable
 */
#define PMEMPOOL_TRANSFORM_DRY_RUN	(1U << 1)

/*
 * PMEMPOOL_MAJOR_VERSION and PMEMPOOL_MINOR_VERSION provide the current version
 * of the libpmempool API as provided by this header file.  Applications can
 * verify that the version available at run-time is compatible with the version
 * used at compile-time by passing these defines to pmempool_check_version().
 */
#define PMEMPOOL_MAJOR_VERSION 1
#define PMEMPOOL_MINOR_VERSION 3

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
	unsigned flags;
};

#ifndef _WIN32
#define pmempool_check_args pmempool_check_argsU
#else
struct pmempool_check_argsW {
	const wchar_t *path;
	const wchar_t *backup_path;
	enum pmempool_pool_type pool_type;
	unsigned flags;
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

/* PMEMPOOL feature enable, disable, query */

/*
 * feature types
 */
enum pmempool_feature {
	PMEMPOOL_FEAT_SINGLEHDR,
	PMEMPOOL_FEAT_CKSUM_2K,
	PMEMPOOL_FEAT_SHUTDOWN_STATE,
	PMEMPOOL_FEAT_CHECK_BAD_BLOCKS,
};

/* PMEMPOOL FEATURE ENABLE */
#ifndef _WIN32
int pmempool_feature_enable(const char *path, enum pmempool_feature feature,
	unsigned flags);
#else
int pmempool_feature_enableU(const char *path, enum pmempool_feature feature,
	unsigned flags);
int pmempool_feature_enableW(const wchar_t *path,
	enum pmempool_feature feature, unsigned flags);
#endif

/* PMEMPOOL FEATURE DISABLE */
#ifndef _WIN32
int pmempool_feature_disable(const char *path, enum pmempool_feature feature,
	unsigned flags);
#else
int pmempool_feature_disableU(const char *path, enum pmempool_feature feature,
	unsigned flags);
int pmempool_feature_disableW(const wchar_t *path,
	enum pmempool_feature feature, unsigned flags);
#endif

/* PMEMPOOL FEATURE QUERY */
#ifndef _WIN32
int pmempool_feature_query(const char *path, enum pmempool_feature feature,
	unsigned flags);
#else
int pmempool_feature_queryU(const char *path, enum pmempool_feature feature,
	unsigned flags);
int pmempool_feature_queryW(const wchar_t *path,
	enum pmempool_feature feature, unsigned flags);
#endif

/* PMEMPOOL RM */
#ifndef _WIN32
int pmempool_rm(const char *path, unsigned flags);
#else
int pmempool_rmU(const char *path, unsigned flags);
int pmempool_rmW(const wchar_t *path, unsigned flags);
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
