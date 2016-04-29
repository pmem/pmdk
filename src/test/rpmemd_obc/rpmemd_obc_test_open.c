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
 * rpmemd_obc_test_open.c -- test cases for open request message
 */

#include "rpmemd_obc_test_common.h"

/*
 * Number of cases for checking open request message. Must be kept in sync
 * with client_bad_msg_open function.
 */
#define BAD_MSG_OPEN_COUNT	11

/*
 * client_bad_msg_open -- check if server detects invalid open request
 * messages
 */
static void
client_bad_msg_open(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(OPEN_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_open *msg = MALLOC(msg_size);

	for (int i = 0; i < BAD_MSG_OPEN_COUNT; i++) {
		int fd = clnt_connect_wait(target);
		*msg = OPEN_MSG;
		msg->hdr.size = msg_size;
		memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);

		switch (i) {
		case 0:
			msg->provider = 0;
			break;
		case 1:
			msg->provider = MAX_RPMEM_PROV;
			break;
		case 2:
			msg->pool_desc.size -= 1;
			break;
		case 3:
			msg->pool_desc.size += 1;
			break;
		case 4:
			msg->pool_desc.size = 0;
			msg->hdr.size = sizeof(OPEN_MSG) +
				msg->pool_desc.size;
			break;
		case 5:
			msg->pool_desc.size = 1;
			msg->hdr.size = sizeof(OPEN_MSG) +
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
			msg->major = RPMEM_PROTO_MAJOR + 1;
			break;
		case 10:
			msg->minor = RPMEM_PROTO_MINOR + 1;
			break;
		default:
			UT_ASSERT(0);
		}

		rpmem_hton_msg_open(msg);

		clnt_send(fd, msg, msg_size);
		clnt_wait_disconnect(fd);
		clnt_close(fd);
	}

	FREE(msg);
	FREE(target);
}

/*
 * client_msg_open_noresp -- send open request message and don't expect a
 * response
 */
static void
client_msg_open_noresp(const char *ctarget)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(OPEN_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_open *msg = MALLOC(msg_size);

	int fd = clnt_connect_wait(target);

	*msg = OPEN_MSG;
	msg->hdr.size = msg_size;
	memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);

	rpmem_hton_msg_open(msg);

	clnt_send(fd, msg, msg_size);
	clnt_wait_disconnect(fd);
	clnt_close(fd);

	FREE(msg);
	FREE(target);
}

/*
 * client_msg_open_resp -- send open request message and expect a response
 * with specified status. If status is 0, validate open request response
 * message
 */
static void
client_msg_open_resp(const char *ctarget, int status)
{
	char *target = STRDUP(ctarget);
	size_t msg_size = sizeof(OPEN_MSG) + POOL_DESC_SIZE;
	struct rpmem_msg_open *msg = MALLOC(msg_size);
	struct rpmem_msg_open_resp resp;

	int fd = clnt_connect_wait(target);

	*msg = OPEN_MSG;
	msg->hdr.size = msg_size;
	memcpy(msg->pool_desc.desc, POOL_DESC, POOL_DESC_SIZE);

	rpmem_hton_msg_open(msg);

	clnt_send(fd, msg, msg_size);
	clnt_recv(fd, &resp, sizeof(resp));
	rpmem_ntoh_msg_open_resp(&resp);

	if (status) {
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);
	} else {
		UT_ASSERTeq(resp.hdr.type, RPMEM_MSG_TYPE_OPEN_RESP);
		UT_ASSERTeq(resp.hdr.size,
				sizeof(struct rpmem_msg_open_resp));
		UT_ASSERTeq(resp.hdr.status, (uint32_t)status);
		UT_ASSERTeq(resp.ibc.port, PORT);
		UT_ASSERTeq(resp.ibc.rkey, RKEY);
		UT_ASSERTeq(resp.ibc.raddr, RADDR);
		UT_ASSERTeq(resp.ibc.persist_method, PERSIST_METHOD);
	}

	clnt_close(fd);

	FREE(msg);
	FREE(target);
}

/*
 * client_open -- test case for open request message - client side
 */
void
client_open(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	client_bad_msg_open(target);

	client_msg_open_noresp(target);

	client_msg_open_resp(target, 0);
	client_msg_open_resp(target, 1);
}

/*
 * server_open -- test case for open request message - server side
 */
void
server_open(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 2)
		UT_FATAL("usage: %s <addr> <port>", tc->name);

	char *node = argv[0];
	char *service = argv[1];

	struct rpmemd_obc *rpdc;
	int ret;

	rpdc = rpmemd_obc_init();
	UT_ASSERTne(rpdc, NULL);

	ret = rpmemd_obc_listen(rpdc, 1, node, service);
	UT_ASSERTeq(ret, 0);

	server_bad_msg(rpdc, BAD_MSG_OPEN_COUNT);

	server_msg_noresp(rpdc, RPMEM_MSG_TYPE_OPEN);

	server_msg_resp(rpdc, RPMEM_MSG_TYPE_OPEN, 0);
	server_msg_resp(rpdc, RPMEM_MSG_TYPE_OPEN, 1);

	ret = rpmemd_obc_close(rpdc);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_fini(rpdc);
}
