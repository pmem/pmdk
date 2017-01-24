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
 * rpmem_obc_test_close.c -- test cases for rpmem_obj_close function
 */

#include "rpmem_obc_test_common.h"

static const struct rpmem_msg_close_resp CLOSE_RESP = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_CLOSE_RESP,
		.size = sizeof(struct rpmem_msg_close_resp),
		.status = 0,
	},
};

/*
 * check_close_msg -- check close message
 */
static void
check_close_msg(struct rpmem_msg_close *msg)
{
	size_t msg_size = sizeof(struct rpmem_msg_close);

	UT_ASSERTeq(msg->hdr.type, RPMEM_MSG_TYPE_CLOSE);
	UT_ASSERTeq(msg->hdr.size, msg_size);
}

/*
 * server_close_handle -- handle a close request message
 */
static void
server_close_handle(struct server *s, const struct rpmem_msg_close_resp *resp)
{
	struct rpmem_msg_close msg;

	srv_recv(s, &msg, sizeof(msg));
	rpmem_ntoh_msg_close(&msg);
	check_close_msg(&msg);
	srv_send(s, resp, sizeof(*resp));
}

/*
 * client_close_errno -- perform close request operation and expect
 * specified errno
 */
static void
client_close_errno(char *target, int ex_errno)
{
	int ret;

	struct rpmem_obc *rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	client_connect_wait(rpc, target);

	ret = rpmem_obc_close(rpc);
	if (ex_errno) {
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);
	} else {
		UT_ASSERTeq(ret, 0);
	}

	rpmem_obc_disconnect(rpc);

	rpmem_obc_fini(rpc);
}

/*
 * Number of cases for EPROTO test. Must be kept in sync with the
 * server_close_eproto function.
 */
#define CLOSE_EPROTO_COUNT 5

/*
 * server_close_eproto -- send invalid create request responses to a client
 */
int
server_close_eproto(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, CLOSE_EPROTO_COUNT - 1);

	int i = atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_close_resp resp = CLOSE_RESP;

	switch (i) {
	case 0:
		resp.hdr.type = MAX_RPMEM_MSG_TYPE;
		break;
	case 1:
		resp.hdr.type = RPMEM_MSG_TYPE_OPEN_RESP;
		break;
	case 2:
		resp.hdr.size -= 1;
		break;
	case 3:
		resp.hdr.size += 1;
		break;
	case 4:
		resp.hdr.status = MAX_RPMEM_ERR;
		break;
	default:
		UT_ASSERT(0);
		break;
	}

	rpmem_hton_msg_close_resp(&resp);

	server_close_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * client_close_error -- check if valid errno is set if error status returned
 */
static void
client_close_error(char *target)
{
	int ret;

	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		set_rpmem_cmd("server_close_error %d", e);

		int ex_errno = rpmem_util_proto_errno(e);
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		client_connect_wait(rpc, target);

		ret = rpmem_obc_close(rpc);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);

		rpmem_obc_disconnect(rpc);

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_close -- test case for close request operation - client side
 */
int
client_close(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < ECONNRESET_LOOP; i++) {
		set_rpmem_cmd("server_close_econnreset %d", i % 2);

		client_close_errno(target, ECONNRESET);
	}

	for (int i = 0; i < CLOSE_EPROTO_COUNT; i++) {
		set_rpmem_cmd("server_close_eproto %d", i);

		client_close_errno(target, EPROTO);
	}

	client_close_error(target);

	set_rpmem_cmd("server_close");

	client_close_errno(target, 0);

	return 1;
}

/*
 * server_close_error -- return error status in close response message
 */
int
server_close_error(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, MAX_RPMEM_ERR);

	enum rpmem_err e = (enum rpmem_err)atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_close_resp resp = CLOSE_RESP;
	resp.hdr.status = e;
	rpmem_hton_msg_close_resp(&resp);
	server_close_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * server_close_econnreset -- test case for closing connection - server size
 */
int
server_close_econnreset(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0|1", tc->name);

	int do_send = atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_close_resp resp = CLOSE_RESP;
	rpmem_hton_msg_close_resp(&resp);

	if (do_send)
		srv_send(s, &resp, sizeof(resp) / 2);

	srv_fini(s);

	return 1;
}

/*
 * server_close -- test case for close request operation - server side
 */
int
server_close(const struct test_case *tc, int argc, char *argv[])
{
	struct server *s = srv_init();

	struct rpmem_msg_close_resp resp = CLOSE_RESP;
	rpmem_hton_msg_close_resp(&resp);

	server_close_handle(s, &resp);

	srv_fini(s);

	return 0;
}
