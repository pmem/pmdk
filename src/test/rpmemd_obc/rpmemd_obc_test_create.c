/*
 * Copyright 2016-2018, Intel Corporation
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
 * rpmemd_obc_test_create.c -- test cases for create request message
 */

#include "rpmemd_obc_test_common.h"

/*
 * Number of cases for checking create request message. Must be kept in sync
 * with client_bad_msg_create function.
 */
#define BAD_MSG_CREATE_COUNT	11

/*
 * client_bad_msg_create -- check if server detects invalid create request
 * messages
 */
static void
client_bad_msg_create(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(CREATE_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_create *msg = MALLOC(msg_size);

	for (int i = 0; i < BAD_MSG_CREATE_COUNT; i++) {
		struct rpmem_ssh *ssh = clnt_connect(target);
		*msg = CREATE_MSG;
		msg->hdr.size = msg_size;
		memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);

		switch (i) {
		case 0:
			msg->c.provider = 0;
			break;
		case 1:
			msg->c.provider = MAX_RPMEM_PROV;
			break;
		case 2:
			msg->pool_desc.size -= 1;
			break;
		case 3:
			msg->pool_desc.size += 1;
			break;
		case 4:
			msg->pool_desc.size = 0;
			msg->hdr.size = sizeof(CREATE_MSG) +
				msg->pool_desc.size;
			break;
		case 5:
			msg->pool_desc.size = 1;
			msg->hdr.size = sizeof(CREATE_MSG) +
				msg->pool_desc.size;
			break;
		case 6:
			msg->pool_desc.desc[0] = '\0';
			break;
		case 7:
			msg->pool_desc.desc[POOL_DESC_SIZE / 2] = '\0';
			break;
		case 8:
			msg->pool_desc.desc[POOL_DESC_SIZE - 1] = 'E';
			break;
		case 9:
			msg->c.major = RPMEM_PROTO_MAJOR + 1;
			break;
		case 10:
			msg->c.minor = RPMEM_PROTO_MINOR + 1;
			break;
		default:
			UT_ASSERT(0);
		}

		rpmem_hton_msg_create(msg);

		clnt_send(ssh, msg, msg_size);
		clnt_wait_disconnect(ssh);
		clnt_close(ssh);
	}

	FREE(msg);
	FREE(target);
}

/*
 * client_msg_create_noresp -- send create request message and don't expect
 * a response
 */
static void
client_msg_create_noresp(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(CREATE_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_create *msg = MALLOC(msg_size);

	struct rpmem_ssh *ssh = clnt_connect(target);

	*msg = CREATE_MSG;
	msg->hdr.size = msg_size;
	memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);

	rpmem_hton_msg_create(msg);

	clnt_send(ssh, msg, msg_size);
	clnt_close(ssh);

	FREE(msg);
	FREE(target);
}

/*
 * client_msg_create_resp -- send create request message and expect a response
 * with specified status. If status is 0, validate create request response
 * message
 */
static void
client_msg_create_resp(const char *ctarget, int status)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(CREATE_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_create *msg = MALLOC(msg_size);
	struct rpmem_msg_create_resp resp;

	struct rpmem_ssh *ssh = clnt_connect(target);

	*msg = CREATE_MSG;
	msg->hdr.size = msg_size;
	memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);

	rpmem_hton_msg_create(msg);

	clnt_send(ssh, msg, msg_size);
	clnt_recv(ssh, &resp, sizeof(resp));
	rpmem_ntoh_msg_create_resp(&resp);

	if (status) {
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);
	} else {
		UT_ASSERTeq(resp.hdr.type, RPMEM_MSG_TYPE_CREATE_RESP);
		UT_ASSERTeq(resp.hdr.size,
				sizeof(struct rpmem_msg_create_resp));
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);
		UT_ASSERTeq(resp.ibc.port, PORT);
		UT_ASSERTeq(resp.ibc.rkey, RKEY);
		UT_ASSERTeq(resp.ibc.raddr, RADDR);
		UT_ASSERTeq(resp.ibc.persist_method, PERSIST_METHOD);
	}

	clnt_close(ssh);

	FREE(msg);
	FREE(target);
}

/*
 * client_create -- test case for create request message - client side
 */
int
client_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	set_rpmem_cmd("server_bad_msg");
	client_bad_msg_create(target);

	set_rpmem_cmd("server_msg_noresp %d", RPMEM_MSG_TYPE_CREATE);
	client_msg_create_noresp(target);

	set_rpmem_cmd("server_msg_resp %d %d", RPMEM_MSG_TYPE_CREATE, 0);
	client_msg_create_resp(target, 0);

	set_rpmem_cmd("server_msg_resp %d %d", RPMEM_MSG_TYPE_CREATE, 1);
	client_msg_create_resp(target, 1);

	return 1;
}
