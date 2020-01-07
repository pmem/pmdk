// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2017, Intel Corporation */

/*
 * rpmemd_obc_test.c -- unit test for rpmemd_obc module
 */

#include "rpmemd_obc_test_common.h"
#include "out.h"
#include "os.h"

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(server_bad_msg),
	TEST_CASE(server_msg_noresp),
	TEST_CASE(server_msg_resp),

	TEST_CASE(client_bad_msg_hdr),

	TEST_CASE(server_econnreset),
	TEST_CASE(client_econnreset),

	TEST_CASE(client_create),
	TEST_CASE(client_open),
	TEST_CASE(client_close),

	TEST_CASE(client_set_attr),
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmemd_obc");

	out_init("rpmemd_obc",
		"RPMEM_LOG_LEVEL",
		"RPMEM_LOG_FILE", 0, 0);
	rpmemd_log_init("rpmemd", os_getenv("RPMEMD_LOG_FILE"), 0);
	rpmemd_log_level = rpmemd_log_level_from_str(
			os_getenv("RPMEMD_LOG_LEVEL"));
	rpmem_util_cmds_init();

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	rpmem_util_cmds_fini();
	rpmemd_log_close();

	out_fini();
	DONE(NULL);
}
