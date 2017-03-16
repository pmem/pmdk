/*
 * Copyright 2014-2017, Intel Corporation
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
 * out_err_win.c -- unit test for error messages
 */

#define LOG_PREFIX "trace"
#define LOG_LEVEL_VAR "TRACE_LOG_LEVEL"
#define LOG_FILE_VAR "TRACE_LOG_FILE"
#define MAJOR_VERSION 1
#define MINOR_VERSION 0

#include <sys/types.h>
#include <stdarg.h>
#include "unittest.h"
#include "pmemcommon.h"

int
wmain(int argc, wchar_t *argv[])
{
	char buff[UT_MAX_ERR_MSG];

	STARTW(argc, argv, "out_err_win");

	/* Execute test */
	common_init(LOG_PREFIX, LOG_LEVEL_VAR, LOG_FILE_VAR,
			MAJOR_VERSION, MINOR_VERSION);

	errno = 0;
	ERR("ERR #%d", 1);
	UT_OUT("%S", out_get_errormsgW());

	errno = 0;
	ERR("!ERR #%d", 2);
	UT_OUT("%S", out_get_errormsgW());

	errno = EINVAL;
	ERR("!ERR #%d", 3);
	UT_OUT("%S", out_get_errormsgW());

	errno = EBADF;
	ut_strerror(errno, buff, UT_MAX_ERR_MSG);
	out_err(__FILE__, 100, __func__,
		"ERR1: %s:%d", buff, 1234);
	UT_OUT("%S", out_get_errormsgW());

	errno = EBADF;
	ut_strerror(errno, buff, UT_MAX_ERR_MSG);
	out_err(NULL, 0, NULL,
		"ERR2: %s:%d", buff, 1234);
	UT_OUT("%S", out_get_errormsgW());

	/* Cleanup */
	common_fini();

	DONEW(NULL);
}
