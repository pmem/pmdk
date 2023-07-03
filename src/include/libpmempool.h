/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2023, Intel Corporation */

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

#ifdef __cplusplus
extern "C" {
#endif

/* PMEMPOOL CHECK */

/*
 * pool types
 */
enum pmempool_pool_type {
	PMEMPOOL_POOL_TYPE_DETECT,
	PMEMPOOL_POOL_TYPE_RESERVED1, /* used to be log */
	PMEMPOOL_POOL_TYPE_RESERVED2, /* used to be blk */
	PMEMPOOL_POOL_TYPE_OBJ,
	PMEMPOOL_POOL_TYPE_RESERVED3, /* used to be btt */
	PMEMPOOL_POOL_TYPE_RESERVED4, /* used to be cto */
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

#define pmempool_check_status pmempool_check_statusU

/*
 * check context arguments
 */
struct pmempool_check_argsU {
	const char *path;
	const char *backup_path;
	enum pmempool_pool_type pool_type;
	unsigned flags;
};

#define pmempool_check_args pmempool_check_argsU

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
 * LIBPMEMPOOL SYNC & TRANSFORM
 */

/*
 * Synchronize data between replicas within a poolset.
 *
 * EXPERIMENTAL
 */
int pmempool_sync(const char *poolset_file, unsigned flags);

/*
 * Modify internal structure of a poolset.
 *
 * EXPERIMENTAL
 */
int pmempool_transform(const char *poolset_file_src,
	const char *poolset_file_dst, unsigned flags);

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
int pmempool_feature_enable(const char *path, enum pmempool_feature feature,
	unsigned flags);

/* PMEMPOOL FEATURE DISABLE */
int pmempool_feature_disable(const char *path, enum pmempool_feature feature,
	unsigned flags);

/* PMEMPOOL FEATURE QUERY */
int pmempool_feature_query(const char *path, enum pmempool_feature feature,
	unsigned flags);

/* PMEMPOOL RM */
int pmempool_rm(const char *path, unsigned flags);

const char *pmempool_check_version(unsigned major_required,
	unsigned minor_required);

const char *pmempool_errormsg(void);

#ifdef __cplusplus
}
#endif
#endif	/* libpmempool.h */
