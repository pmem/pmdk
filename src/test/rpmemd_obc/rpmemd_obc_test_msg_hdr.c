// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * rpmemd_obc_test_msg_hdr.c -- test cases for message header
 */

#include "rpmemd_obc_test_common.h"

/*
 * Number of cases for checking message header. Must be kept in sync with
 * client_bad_msg_hdr function.
 */
#define BAD_MSG_HDR_COUNT	6

/*
 * client_bad_msg_hdr -- test case for checking message header
 */
int
client_bad_msg_hdr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	set_rpmem_cmd("server_bad_msg");

	for (int i = 0; i < BAD_MSG_HDR_COUNT; i++) {
		struct rpmem_ssh *ssh = clnt_connect(target);

		struct rpmem_msg_hdr msg = MSG_HDR;

		switch (i) {
		case 0:
			msg.size -= 1;
			break;
		case 1:
			msg.size = 0;
			break;
		case 2:
			msg.type = MAX_RPMEM_MSG_TYPE;
			break;
		case 3:
			msg.type = RPMEM_MSG_TYPE_OPEN_RESP;
			break;
		case 4:
			msg.type = RPMEM_MSG_TYPE_CREATE_RESP;
			break;
		case 5:
			msg.type = RPMEM_MSG_TYPE_CLOSE_RESP;
			break;
		default:
			UT_ASSERT(0);
		}

		rpmem_hton_msg_hdr(&msg);

		clnt_send(ssh, &msg, sizeof(msg));
		clnt_wait_disconnect(ssh);
		clnt_close(ssh);
	}

	return 1;
}
