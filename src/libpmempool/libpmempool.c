// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2023, Intel Corporation */

/*
 * libpmempool.c -- entry points for libpmempool
 */

#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <sys/param.h>

#include "pmemcommon.h"
#include "libpmempool.h"
#include "pmempool.h"
#include "pool.h"
#include "check.h"

/*
 * libpmempool_init -- load-time initialization for libpmempool
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmempool_init(void)
{
	common_init(PMEMPOOL_LOG_PREFIX, PMEMPOOL_LOG_LEVEL_VAR,
		PMEMPOOL_LOG_FILE_VAR, PMEMPOOL_MAJOR_VERSION,
		PMEMPOOL_MINOR_VERSION);
	LOG(3, NULL);
}

/*
 * libpmempool_fini -- libpmempool cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmempool_fini(void)
{
	LOG(3, NULL);
	common_fini();
}

/*
 * pmempool_check_versionU -- see if library meets application version
 *	requirements
 */
static inline
const char *
pmempool_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEMPOOL_MAJOR_VERSION) {
		ERR("libpmempool major version mismatch (need %u, found %u)",
			major_required, PMEMPOOL_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > PMEMPOOL_MINOR_VERSION) {
		ERR("libpmempool minor version mismatch (need %u, found %u)",
			minor_required, PMEMPOOL_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

/*
 * pmempool_check_version -- see if lib meets application version requirements
 */
const char *
pmempool_check_version(unsigned major_required, unsigned minor_required)
{
	return pmempool_check_versionU(major_required, minor_required);
}

/*
 * pmempool_errormsgU -- return last error message
 */
static inline
const char *
pmempool_errormsgU(void)
{
	return out_get_errormsg();
}

/*
 * pmempool_errormsg -- return last error message
 */
const char *
pmempool_errormsg(void)
{
	return pmempool_errormsgU();
}

/*
 * pmempool_ppc_set_default -- (internal) set default values of check context
 */
static void
pmempool_ppc_set_default(PMEMpoolcheck *ppc)
{
	/* all other fields should be zeroed */
	const PMEMpoolcheck ppc_default = {
		.args		= {
			.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
		},
		.result		= CHECK_RESULT_CONSISTENT,
	};
	*ppc = ppc_default;
}

/*
 * pmempool_check_initU -- initialize check context
 */
static inline
PMEMpoolcheck *
pmempool_check_initU(struct pmempool_check_argsU *args, size_t args_size)
{
	LOG(3, "path %s backup_path %s pool_type %u flags %x", args->path,
		args->backup_path, args->pool_type, args->flags);

	/*
	 * Currently one size of args structure is supported. The version of the
	 * pmempool_check_args structure can be distinguished based on provided
	 * args_size.
	 */
	if (args_size < sizeof(struct pmempool_check_args)) {
		ERR("provided args_size is not supported");
		errno = EINVAL;
		return NULL;
	}

	/*
	 * Dry run does not allow to made changes possibly performed during
	 * repair. Advanced allow to perform more complex repairs. Questions
	 * are ask only if repairs are made. So dry run, advanced and always_yes
	 * can be set only if repair is set.
	 */
	if (util_flag_isclr(args->flags, PMEMPOOL_CHECK_REPAIR) &&
			util_flag_isset(args->flags, PMEMPOOL_CHECK_DRY_RUN |
			PMEMPOOL_CHECK_ADVANCED | PMEMPOOL_CHECK_ALWAYS_YES)) {
		ERR("dry_run, advanced and always_yes are applicable only if "
			"repair is set");
		errno = EINVAL;
		return NULL;
	}

	/*
	 * dry run does not modify anything so performing backup is redundant
	 */
	if (util_flag_isset(args->flags, PMEMPOOL_CHECK_DRY_RUN) &&
			args->backup_path != NULL) {
		ERR("dry run does not allow one to perform backup");
		errno = EINVAL;
		return NULL;
	}

	/*
	 * libpmempool uses str format of communication so it must be set
	 */
	if (util_flag_isclr(args->flags, PMEMPOOL_CHECK_FORMAT_STR)) {
		ERR("PMEMPOOL_CHECK_FORMAT_STR flag must be set");
		errno = EINVAL;
		return NULL;
	}

	PMEMpoolcheck *ppc = calloc(1, sizeof(*ppc));
	if (ppc == NULL) {
		ERR("!calloc");
		return NULL;
	}

	pmempool_ppc_set_default(ppc);
	memcpy(&ppc->args, args, sizeof(ppc->args));
	ppc->path = strdup(args->path);
	if (!ppc->path) {
		ERR("!strdup");
		goto error_path_malloc;
	}
	ppc->args.path = ppc->path;

	if (args->backup_path != NULL) {
		ppc->backup_path = strdup(args->backup_path);
		if (!ppc->backup_path) {
			ERR("!strdup");
			goto error_backup_path_malloc;
		}
		ppc->args.backup_path = ppc->backup_path;
	}

	if (check_init(ppc) != 0)
		goto error_check_init;

	return ppc;

error_check_init:
	/* in case errno not set by any of the used functions set its value */
	if (errno == 0)
		errno = EINVAL;

	free(ppc->backup_path);
error_backup_path_malloc:
	free(ppc->path);
error_path_malloc:
	free(ppc);
	return NULL;
}

/*
 * pmempool_check_init -- initialize check context
 */
PMEMpoolcheck *
pmempool_check_init(struct pmempool_check_args *args, size_t args_size)
{
	return pmempool_check_initU(args, args_size);
}

/*
 * pmempool_checkU -- continue check till produce status to consume for caller
 */
static inline
struct pmempool_check_statusU *
pmempool_checkU(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);
	ASSERTne(ppc, NULL);

	struct check_status *result;
	do {
		result = check_step(ppc);

		if (check_is_end(ppc->data) && result == NULL)
			return NULL;
	} while (result == NULL);

	return check_status_get(result);
}

/*
 * pmempool_check -- continue check till produce status to consume for caller
 */
struct pmempool_check_status *
pmempool_check(PMEMpoolcheck *ppc)
{
	return pmempool_checkU(ppc);
}

/*
 * pmempool_check_end -- end check and release check context
 */
enum pmempool_check_result
pmempool_check_end(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);
	const enum check_result result = ppc->result;
	const unsigned sync_required = ppc->sync_required;

	check_fini(ppc);
	free(ppc->path);
	free(ppc->backup_path);
	free(ppc);

	if (sync_required) {
		switch (result) {
		case CHECK_RESULT_CONSISTENT:
		case CHECK_RESULT_REPAIRED:
			return PMEMPOOL_CHECK_RESULT_SYNC_REQ;
		default:
			/* other results require fixing prior to sync */
			;
		}
	}

	switch (result) {
		case CHECK_RESULT_CONSISTENT:
			return PMEMPOOL_CHECK_RESULT_CONSISTENT;

		case CHECK_RESULT_NOT_CONSISTENT:
			return PMEMPOOL_CHECK_RESULT_NOT_CONSISTENT;

		case CHECK_RESULT_REPAIRED:
			return PMEMPOOL_CHECK_RESULT_REPAIRED;

		case CHECK_RESULT_CANNOT_REPAIR:
			return PMEMPOOL_CHECK_RESULT_CANNOT_REPAIR;

		default:
			return PMEMPOOL_CHECK_RESULT_ERROR;
	}
}
