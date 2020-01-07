// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

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
