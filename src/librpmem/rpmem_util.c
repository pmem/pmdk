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
 * rpmem_util.c -- util functions for librpmem source file
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>

#include "out.h"
#include "util.h"
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
		.err	= ENOMEM,
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
		.str	= "Pool set doesn't exist",
	},
	[RPMEM_ERR_NOACCESS] = {
		.err	= EACCES,
		.str	= "Pool set permission denied",
	},
};

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
