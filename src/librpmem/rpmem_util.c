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

static struct rpmem_err_str_errno {
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
static char *Rpmem_current_cmd;

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

	Rpmem_current_cmd = Rpmem_cmds;
}

/*
 * rpmem_util_env_fini -- release RPMEM_CMD copy
 */
void
rpmem_util_cmds_fini(void)
{
	RPMEM_ASSERT(Rpmem_cmds);

	free(Rpmem_cmds);
	Rpmem_cmds = NULL;
	Rpmem_current_cmd = NULL;
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
	RPMEM_ASSERT(Rpmem_current_cmd);

	char *eol = strchr(Rpmem_current_cmd, RPMEM_CMD_SEPARATOR);
	char *ret = Rpmem_current_cmd;
	if (eol) {
		*eol = '\0';
		Rpmem_current_cmd = eol + 1;
	}

	return ret;
}

/*
 * rpmem_util_get_env_max_nlanes -- read the maximum number of lanes from
 * RPMEM_MAX_NLANES
 */
void
rpmem_util_get_env_max_nlanes(unsigned *max_nlanes)
{
	char *env_nlanes = os_getenv(RPMEM_MAX_NLANES_ENV);
	if (env_nlanes) {
		int nlanes = atoi(env_nlanes);
		if (nlanes <= 0) {
			RPMEM_LOG(ERR, "%s variable must be a positive integer",
					RPMEM_MAX_NLANES_ENV);
		} else {
			*max_nlanes = (unsigned)nlanes;
		}
	}
}
