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
 * rpmem_obc_test.c -- unit test for rpmem_obc module
 */

#include "rpmem_obc_test_common.h"
#include "pmemcommon.h"

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(client_enotconn),
	TEST_CASE(client_connect),

	TEST_CASE(client_create),
	TEST_CASE(server_create),
	TEST_CASE(server_create_econnreset),
	TEST_CASE(server_create_eproto),
	TEST_CASE(server_create_error),

	TEST_CASE(client_open),
	TEST_CASE(server_open),
	TEST_CASE(server_open_econnreset),
	TEST_CASE(server_open_eproto),
	TEST_CASE(server_open_error),

	TEST_CASE(client_close),
	TEST_CASE(server_close),
	TEST_CASE(server_close_econnreset),
	TEST_CASE(server_close_eproto),
	TEST_CASE(server_close_error),

	TEST_CASE(client_monitor),
	TEST_CASE(server_monitor),

	TEST_CASE(client_set_attr),
	TEST_CASE(server_set_attr),
	TEST_CASE(server_set_attr_econnreset),
	TEST_CASE(server_set_attr_eproto),
	TEST_CASE(server_set_attr_error),
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmem_obc");
	common_init("rpmem_obc",
		"RPMEM_LOG_LEVEL",
		"RPMEM_LOG_FILE", 0, 0);

	rpmem_util_cmds_init();

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	rpmem_util_cmds_fini();
	common_fini();

	DONE(NULL);
}
