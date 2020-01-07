// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * rpmem_util.c -- util functions for librpmem source file
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "out.h"
#include "os.h"
#include "librpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_util.h"

static const struct rpmem_err_str_errno {
	int err;
	const char *str;
} rpmem_err_str_errno[MAX_RPMEM_ERR] = {
	[RPMEM_SUCCESS] = {
		.err	= 0,
		.str	= "Success",
	},
	[RPMEM_ERR_BADPROTO] = {
		.err	= EPROTONOSUPPORT,
		.str	= "Protocol version number mismatch",
	},
	[RPMEM_ERR_BADNAME] = {
		.err	= EINVAL,
		.str	= "Invalid pool descriptor",
	},
	[RPMEM_ERR_BADSIZE] = {
		.err	= EFBIG,
		.str	= "Invalid pool size",
	},
	[RPMEM_ERR_BADNLANES] = {
		.err	= EINVAL,
		.str	= "Invalid number of lanes",
	},
	[RPMEM_ERR_BADPROVIDER] = {
		.err	= EINVAL,
		.str	= "Invalid provider",
	},
	[RPMEM_ERR_FATAL] = {
		.err	= EREMOTEIO,
		.str	= "Fatal error",
	},
	[RPMEM_ERR_FATAL_CONN] = {
		.err	= ECONNABORTED,
		.str	= "Fatal in-band connection error",
	},
	[RPMEM_ERR_BUSY] = {
		.err	= EBUSY,
		.str	= "Pool already in use",
	},
	[RPMEM_ERR_EXISTS] = {
		.err	= EEXIST,
		.str	= "Pool already exists",
	},
	[RPMEM_ERR_PROVNOSUP] = {
		.err	= EMEDIUMTYPE,
		.str	= "Provider not supported",
	},
	[RPMEM_ERR_NOEXIST] = {
		.err	= ENOENT,
		.str	= "Pool set or its part doesn't exist or it is "
				"unavailable",
	},
	[RPMEM_ERR_NOACCESS] = {
		.err	= EACCES,
		.str	= "Pool set permission denied",
	},
	[RPMEM_ERR_POOL_CFG] = {
		.err	= EINVAL,
		.str	= "Invalid pool set configuration",
	},
};

static char *Rpmem_cmds;
static char **Rpmem_cmd_arr;
static size_t Rpmem_current_cmd;
static size_t Rpmem_ncmds;

#define RPMEM_CMD_SEPARATOR '|'

/*
 * rpmem_util_proto_errstr -- return error string for error code
 */
const char *
rpmem_util_proto_errstr(enum rpmem_err err)
{
	RPMEM_ASSERT(err < MAX_RPMEM_ERR);

	const char *ret = rpmem_err_str_errno[err].str;
	RPMEM_ASSERT(ret);

	return ret;
}

/*
 * rpmem_util_proto_errno -- return appropriate errno value for error code
 */
int
rpmem_util_proto_errno(enum rpmem_err err)
{
	RPMEM_ASSERT(err < MAX_RPMEM_ERR);

	return rpmem_err_str_errno[err].err;
}

/*
 * rpmem_util_cmds_inc -- increase size of array for rpmem commands
 */
static void
rpmem_util_cmds_inc(void)
{
	Rpmem_ncmds++;
	Rpmem_cmd_arr = realloc(Rpmem_cmd_arr,
			Rpmem_ncmds * sizeof(*Rpmem_cmd_arr));
	if (!Rpmem_cmd_arr)
		RPMEM_FATAL("!realloc");

}

/*
 * rpmem_util_cmds_init -- read a RPMEM_CMD from the environment variable
 */
void
rpmem_util_cmds_init(void)
{
	char *cmd = os_getenv(RPMEM_CMD_ENV);
	if (!cmd)
		cmd = RPMEM_DEF_CMD;

	Rpmem_cmds = strdup(cmd);
	if (!Rpmem_cmds)
		RPMEM_FATAL("!strdup");

	char *next = Rpmem_cmds;
	while (next) {
		rpmem_util_cmds_inc();
		Rpmem_cmd_arr[Rpmem_ncmds - 1] = next;

		next = strchr(next, RPMEM_CMD_SEPARATOR);
		if (next) {
			*next = '\0';
			next++;
		}
	}
}

/*
 * rpmem_util_env_fini -- release RPMEM_CMD copy
 */
void
rpmem_util_cmds_fini(void)
{
	RPMEM_ASSERT(Rpmem_cmds);
	RPMEM_ASSERT(Rpmem_cmd_arr);
	RPMEM_ASSERT(Rpmem_current_cmd < Rpmem_ncmds);

	free(Rpmem_cmds);
	Rpmem_cmds = NULL;

	free(Rpmem_cmd_arr);
	Rpmem_cmd_arr = NULL;

	Rpmem_ncmds = 0;
	Rpmem_current_cmd = 0;
}

/*
 * rpmem_util_cmd_get -- get a next command from RPMEM_CMD
 *
 * RPMEM_CMD can contain multiple commands separated by RPMEM_CMD_SEPARATOR.
 * Commands from RPMEM_CMD are read sequentially and used to establish out of
 * band connections to remote nodes in the order read from a poolset file.
 *
 */
const char *
rpmem_util_cmd_get(void)
{
	RPMEM_ASSERT(Rpmem_cmds);
	RPMEM_ASSERT(Rpmem_cmd_arr);
	RPMEM_ASSERT(Rpmem_current_cmd < Rpmem_ncmds);

	char *ret = Rpmem_cmd_arr[Rpmem_current_cmd];

	Rpmem_current_cmd = (Rpmem_current_cmd + 1) % Rpmem_ncmds;

	return ret;
}

/*
 * rpmem_util_get_env_uint -- read the unsigned value from environment
 */
static void
rpmem_util_get_env_uint(const char *env, unsigned *pval)
{
	char *env_val = os_getenv(env);
	if (env_val && env_val[0] != '\0') {
		char *endptr;
		errno = 0;

		long val = strtol(env_val, &endptr, 10);

		if (endptr[0] != '\0' || val <= 0 ||
			(errno == ERANGE &&
			(val == LONG_MAX || val == LONG_MIN))) {
			RPMEM_LOG(ERR, "%s variable must be a positive integer",
					env);
		} else {
			*pval = val < UINT_MAX ? (unsigned)val: UINT_MAX;
		}
	}
}

/*
 * rpmem_util_get_env_max_nlanes -- read the maximum number of lanes from
 * RPMEM_MAX_NLANES
 */
void
rpmem_util_get_env_max_nlanes(unsigned *max_nlanes)
{
	rpmem_util_get_env_uint(RPMEM_MAX_NLANES_ENV, max_nlanes);
}

/*
 * rpmem_util_get_env_wq_size -- read the required WQ size from env
 */
void
rpmem_util_get_env_wq_size(unsigned *wq_size)
{
	rpmem_util_get_env_uint(RPMEM_WQ_SIZE_ENV, wq_size);
}
