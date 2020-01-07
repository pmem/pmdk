// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * rpmemd_obc_test_set_attr.c -- test cases for set attributes request message
 */

#include "rpmemd_obc_test_common.h"

/*
 * client_msg_set_attr_noresp -- send set attributes request message and don't
 * expect a response
 */
static void
client_msg_set_attr_noresp(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(SET_ATTR_MSG);
	struct rpmem_msg_set_attr *msg = MALLOC(msg_size);

	struct rpmem_ssh *ssh = clnt_connect(target);

	*msg = SET_ATTR_MSG;

	rpmem_hton_msg_set_attr(msg);

	clnt_send(ssh, msg, msg_size);
	clnt_wait_disconnect(ssh);
	clnt_close(ssh);

	FREE(msg);
	FREE(target);
}

/*
 * client_msg_set_attr_resp -- send set attributes request message and expect
 * a response with specified status. If status is 0, validate set attributes
 * request response message
 */
static void
client_msg_set_attr_resp(const char *ctarget, int status)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(SET_ATTR_MSG);
	struct rpmem_msg_set_attr *msg = MALLOC(msg_size);
	struct rpmem_msg_set_attr_resp resp;

	struct rpmem_ssh *ssh = clnt_connect(target);

	*msg = SET_ATTR_MSG;

	rpmem_hton_msg_set_attr(msg);

	clnt_send(ssh, msg, msg_size);
	clnt_recv(ssh, &resp, sizeof(resp));
	rpmem_ntoh_msg_set_attr_resp(&resp);

	if (status) {
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);
	} else {
		UT_ASSERTeq(resp.hdr.type, RPMEM_MSG_TYPE_SET_ATTR_RESP);
		UT_ASSERTeq(resp.hdr.size,
				sizeof(struct rpmem_msg_set_attr_resp));
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);
	}

	clnt_close(ssh);

	FREE(msg);
	FREE(target);
}

/*
 * client_set_attr -- test case for set attributes request message - client
 * side
 */
int
client_set_attr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	set_rpmem_cmd("server_msg_noresp %d", RPMEM_MSG_TYPE_SET_ATTR);
	client_msg_set_attr_noresp(target);

	set_rpmem_cmd("server_msg_resp %d %d", RPMEM_MSG_TYPE_SET_ATTR, 0);
	client_msg_set_attr_resp(target, 0);

	set_rpmem_cmd("server_msg_resp %d %d", RPMEM_MSG_TYPE_SET_ATTR, 1);
	client_msg_set_attr_resp(target, 1);

	return 1;
}
