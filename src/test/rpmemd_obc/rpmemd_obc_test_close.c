// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016, Intel Corporation */

/*
 * rpmemd_obc_test_close.c -- test cases for close request message
 */

#include "rpmemd_obc_test_common.h"

/*
 * client_msg_close_noresp -- send close request message and don't expect a
 * response
 */
static void
client_msg_close_noresp(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	struct rpmem_msg_close msg = CLOSE_MSG;
	rpmem_hton_msg_close(&msg);

	struct rpmem_ssh *ssh = clnt_connect(target);

	clnt_send(ssh, &msg, sizeof(msg));
	clnt_wait_disconnect(ssh);
	clnt_close(ssh);

	FREE(target);
}

/*
 * client_msg_close_resp -- send close request message and expect a response
 * with specified status. If status is 0, validate close request response
 * message
 */
static void
client_msg_close_resp(const char *ctarget, int status)
{
	char *target = STRDUP(ctarget);
	struct rpmem_msg_close msg = CLOSE_MSG;
	rpmem_hton_msg_close(&msg);
	struct rpmem_msg_close_resp resp;

	struct rpmem_ssh *ssh = clnt_connect(target);

	clnt_send(ssh, &msg, sizeof(msg));
	clnt_recv(ssh, &resp, sizeof(resp));
	rpmem_ntoh_msg_close_resp(&resp);

	if (status)
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);

	clnt_close(ssh);

	FREE(target);
}

/*
 * client_close -- test case for close request message - client side
 */
int
client_close(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	set_rpmem_cmd("server_msg_noresp %d", RPMEM_MSG_TYPE_CLOSE);
	client_msg_close_noresp(target);

	set_rpmem_cmd("server_msg_resp %d %d", RPMEM_MSG_TYPE_CLOSE, 0);
	client_msg_close_resp(target, 0);

	set_rpmem_cmd("server_msg_resp %d %d", RPMEM_MSG_TYPE_CLOSE, 1);
	client_msg_close_resp(target, 1);

	return 1;
}
