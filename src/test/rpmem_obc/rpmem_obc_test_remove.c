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
 * rpmem_obc_test_remove.c -- test cases for rpmem_obj_remove function
 */

#include "rpmem_obc_test_common.h"

static const struct rpmem_msg_remove_resp REMOVE_RESP = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_REMOVE_RESP,
		.size = sizeof(struct rpmem_msg_remove_resp),
		.status = 0,
	},
};

/*
 * check_remove_msg -- check remove message
 */
static void
check_remove_msg(struct rpmem_msg_remove *msg)
{
	size_t pool_desc_size = strlen(POOL_DESC) + 1;
	size_t msg_size = sizeof(struct rpmem_msg_remove) + pool_desc_size;

	UT_ASSERTeq(msg->hdr.type, RPMEM_MSG_TYPE_REMOVE);
	UT_ASSERTeq(msg->hdr.size, msg_size);
	UT_ASSERTeq(msg->major, RPMEM_PROTO_MAJOR);
	UT_ASSERTeq(msg->minor, RPMEM_PROTO_MINOR);
	UT_ASSERTeq(msg->pool_desc.size, pool_desc_size);
	UT_ASSERTeq(strcmp((char *)msg->pool_desc.desc, POOL_DESC), 0);
}

/*
 * server_remove_handle -- handle a remove request message
 */
static void
server_remove_handle(struct server *s, const struct rpmem_msg_remove_resp *resp)
{
	size_t pool_desc_size = strlen(POOL_DESC) + 1;
	size_t msg_size = sizeof(struct rpmem_msg_remove) + pool_desc_size;
	struct rpmem_msg_remove *msg = MALLOC(msg_size);
	srv_accept(s);
	srv_recv(s, msg, msg_size);
	rpmem_ntoh_msg_remove(msg);
	check_remove_msg(msg);
	srv_send(s, resp, sizeof(*resp));
	srv_disconnect(s);

	FREE(msg);
}

/*
 * Number of cases for EPROTO test. Must be kept in sync with the
 * server_remove_eproto function.
 */
#define REMOVE_EPROTO_COUNT 5

/*
 * server_remove_eproto -- send invalid remove request responses to a client
 */
static void
server_remove_eproto(struct server *s)
{
	for (int i = 0; i < REMOVE_EPROTO_COUNT; i++) {
		struct rpmem_msg_remove_resp resp = REMOVE_RESP;

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

		rpmem_hton_msg_remove_resp(&resp);

		server_remove_handle(s, &resp);
	}
}

/*
 * server_remove_error -- return error status in remove response message
 */
static void
server_remove_error(struct server *s)
{
	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		struct rpmem_msg_remove_resp resp = REMOVE_RESP;
		resp.hdr.status = e;
		rpmem_hton_msg_remove_resp(&resp);
		server_remove_handle(s, &resp);
	}
}

/*
 * server_remove -- test case for remove request operation - server side
 */
void
server_remove(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s 0|<port>", tc->name);

	unsigned short port = srv_get_port(argv[0]);
	struct server *s = srv_listen(port);

	struct rpmem_msg_remove_resp resp = REMOVE_RESP;
	rpmem_hton_msg_remove_resp(&resp);

	server_econnreset(s, &resp, sizeof(resp) / 2);

	server_remove_eproto(s);

	server_remove_error(s);

	server_remove_handle(s, &resp);

	srv_stop(s);
}

/*
 * client_remove_errno -- perform remove request operation and expect
 * specified errno, repeat the operation specified number of times.
 * If ex_errno is zero expect certain values in res struct.
 */
static void
client_remove_errno(char *target, int ex_errno, int count)
{
	int ret;

	for (int i = 0; i < count; i++) {
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		client_connect_wait(rpc, target);

		ret = rpmem_obc_remove(rpc, POOL_DESC);
		if (ex_errno) {
			UT_ASSERTne(ret, 0);
			UT_ASSERTeq(errno, ex_errno);
		} else {
			UT_ASSERTeq(ret, 0);
		}

		rpmem_obc_disconnect(rpc);

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_remove_error -- check if valid errno is set if error status returned
 */
static void
client_remove_error(char *target)
{
	int ret;

	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		int ex_errno = rpmem_util_proto_errno(e);
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		client_connect_wait(rpc, target);

		ret = rpmem_obc_remove(rpc, POOL_DESC);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);

		rpmem_obc_disconnect(rpc);

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_remove -- test case for remove request operation - client side
 */
void
client_remove(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < ECONNRESET_LOOP; i++)
		client_remove_errno(target, ECONNRESET, ECONNRESET_COUNT);

	client_remove_errno(target, EPROTO, REMOVE_EPROTO_COUNT);

	client_remove_error(target);

	client_remove_errno(target, 0, 1);
}
