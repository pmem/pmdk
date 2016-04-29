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
 * rpmemd_obc_test.c -- unit test for rpmemd_obc module
 */

#include "rpmemd_obc_test_common.h"

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(server_accept_seq),
	TEST_CASE(server_accept_seq_fork),
	TEST_CASE(client_accept_seq),

	TEST_CASE(server_accept_sim),
	TEST_CASE(server_accept_sim_fork),
	TEST_CASE(client_accept_sim),

	TEST_CASE(server_econnreset),
	TEST_CASE(client_econnreset),

	TEST_CASE(server_bad_msg_hdr),
	TEST_CASE(client_bad_msg_hdr),

	TEST_CASE(server_create),
	TEST_CASE(client_create),

	TEST_CASE(server_open),
	TEST_CASE(client_open),

	TEST_CASE(server_close),
	TEST_CASE(client_close),

	TEST_CASE(server_remove),
	TEST_CASE(client_remove),
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "rpmemd_obc");

	rpmemd_log_init("rpmemd", getenv("RPMEMD_LOG_FILE"), 0);
	rpmemd_log_level = rpmemd_log_level_from_str(
			getenv("RPMEMD_LOG_LEVEL"));

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	rpmemd_log_close();

	DONE(NULL);
}
