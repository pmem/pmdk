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

#ifdef USE_RPMEM
#include "rpmem_common.h"
#include "rpmem_util.h"
#endif

#ifdef _WIN32
#define ANSWER_BUFFSIZE 256
#endif

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
#ifdef USE_RPMEM
	util_remote_init();
	rpmem_util_cmds_init();
#endif
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
#ifdef USE_RPMEM
	util_remote_unload();
	util_remote_fini();
	rpmem_util_cmds_fini();
#endif
	common_fini();
}

/*
 * pmempool_check_versionU -- see if library meets application version
 *	requirements
 */
#ifndef _WIN32
static inline
#endif
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

#ifndef _WIN32
/*
 * pmempool_check_version -- see if lib meets application version requirements
 */
const char *
pmempool_check_version(unsigned major_required, unsigned minor_required)
{
	return pmempool_check_versionU(major_required, minor_required);
}
#else
/*
 * pmempool_check_versionW -- see if library meets application version
 *	requirements as widechar
 */
const wchar_t *
pmempool_check_versionW(unsigned major_required, unsigned minor_required)
{
	if (pmempool_check_versionU(major_required, minor_required) != NULL)
		return out_get_errormsgW();
	else
		return NULL;
}
#endif

/*
 * pmempool_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmempool_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmempool_errormsg -- return last error message
 */
const char *
pmempool_errormsg(void)
{
	return pmempool_errormsgU();
}
#else
/*
 * pmempool_errormsgW -- return last error message as widechar
 */
const wchar_t *
pmempool_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif

/*
 * pmempool_ppc_set_default -- (internal) set default values of check context
 */
static void
pmempool_ppc_set_default(PMEMpoolcheck *ppc)
{
	const PMEMpoolcheck ppc_default = {
		.args		= {
			.path		= NULL,
			.backup_path	= NULL,
			.pool_type	= PMEMPOOL_POOL_TYPE_DETECT,
			.flags		= 0
		},
		.data		= NULL,
		.pool		= NULL,
		.result		= CHECK_RESULT_CONSISTENT,
	};
	*ppc = ppc_default;
}

/*
 * pmempool_check_initU -- initialize check context
 */
#ifndef _WIN32
static inline
#endif
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
		ERR("dry run does not allow to perform backup");
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

	PMEMpoolcheck *ppc = malloc(sizeof(*ppc));
	if (ppc == NULL) {
		ERR("!malloc");
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

#ifndef _WIN32
/*
 * pmempool_check_init -- initialize check context
 */
PMEMpoolcheck *
pmempool_check_init(struct pmempool_check_args *args, size_t args_size)
{
	return pmempool_check_initU(args, args_size);
}
#else
/*
 * pmempool_check_initW -- initialize check context as widechar
 */
PMEMpoolcheck *
pmempool_check_initW(struct pmempool_check_argsW *args, size_t args_size)
{
	char *upath = util_toUTF8(args->path);
	if (upath == NULL)
		return NULL;
	char *ubackup_path = NULL;
	if (args->backup_path != NULL) {
		ubackup_path = util_toUTF8(args->backup_path);
		if (ubackup_path == NULL) {
			util_free_UTF8(upath);
			return NULL;
		}
	}

	struct pmempool_check_argsU uargs = {
		.path = upath,
		.backup_path = ubackup_path,
		.pool_type = args->pool_type,
		.flags = args->flags
	};

	PMEMpoolcheck *ret = pmempool_check_initU(&uargs, args_size);

	util_free_UTF8(ubackup_path);
	util_free_UTF8(upath);
	return ret;
}
#endif

/*
 * pmempool_checkU -- continue check till produce status to consume for caller
 */
#ifndef _WIN32
static inline
#endif
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

#ifndef _WIN32
/*
 * pmempool_check -- continue check till produce status to consume for caller
 */
struct pmempool_check_status *
pmempool_check(PMEMpoolcheck *ppc)
{
	return pmempool_checkU(ppc);
}
#else
/*
 * pmempool_checkW -- continue check till produce status to consume for caller
 */
struct pmempool_check_statusW *
pmempool_checkW(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);
	ASSERTne(ppc, NULL);

	/* check the cache and convert msg and answer */
	char buf[ANSWER_BUFFSIZE];
	memset(buf, 0, ANSWER_BUFFSIZE);
	convert_status_cache(ppc, buf, ANSWER_BUFFSIZE);

	struct check_status *uresult;
	do {
		uresult = check_step(ppc);

		if (check_is_end(ppc->data) && uresult == NULL)
			return NULL;
	} while (uresult == NULL);

	struct pmempool_check_statusU *uret_res = check_status_get(uresult);
	const wchar_t *wmsg = util_toUTF16(uret_res->str.msg);
	if (wmsg == NULL)
		FATAL("!malloc");

	struct pmempool_check_statusW *wret_res =
		(struct pmempool_check_statusW *)uret_res;
	/* pointer to old message is freed in next check step */
	wret_res->str.msg = wmsg;
	return wret_res;
}
#endif

/*
 * pmempool_check_end -- end check and release check context
 */
enum pmempool_check_result
pmempool_check_end(PMEMpoolcheck *ppc)
{
	LOG(3, NULL);
	enum check_result result = ppc->result;

	check_fini(ppc);
	free(ppc->path);
	free(ppc->backup_path);
	free(ppc);

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
