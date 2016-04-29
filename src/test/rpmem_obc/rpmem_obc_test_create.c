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
 * rpmem_obc_test_create.c -- test cases for rpmem_obc_create function
 */

#include "rpmem_obc_test_common.h"

static const struct rpmem_msg_create_resp CREATE_RESP = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_CREATE_RESP,
		.size = sizeof(struct rpmem_msg_create_resp),
		.status = 0,
	},
	.ibc	= {
		.port = PORT,
		.rkey = RKEY,
		.raddr = RADDR,
		.persist_method = RPMEM_PM_GPSPM,
		.nlanes = NLANES_RESP,
	},
};

/*
 * check_create_msg -- check create message
 */
static void
check_create_msg(struct rpmem_msg_create *msg)
{
	size_t pool_desc_size = strlen(POOL_DESC) + 1;
	size_t msg_size = sizeof(struct rpmem_msg_create) + pool_desc_size;
	struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

	UT_ASSERTeq(msg->hdr.type, RPMEM_MSG_TYPE_CREATE);
	UT_ASSERTeq(msg->hdr.size, msg_size);
	UT_ASSERTeq(msg->major, RPMEM_PROTO_MAJOR);
	UT_ASSERTeq(msg->minor, RPMEM_PROTO_MINOR);
	UT_ASSERTeq(msg->pool_size, POOL_SIZE);
	UT_ASSERTeq(msg->provider, PROVIDER);
	UT_ASSERTeq(msg->nlanes, NLANES);
	UT_ASSERTeq(msg->pool_desc.size, pool_desc_size);
	UT_ASSERTeq(strcmp((char *)msg->pool_desc.desc, POOL_DESC), 0);
	UT_ASSERTeq(memcmp(&msg->pool_attr, &pool_attr, sizeof(pool_attr)), 0);
}

/*
 * server_create_handle -- handle a create request message
 */
static void
server_create_handle(struct server *s, const struct rpmem_msg_create_resp *resp)
{
	size_t msg_size = sizeof(struct rpmem_msg_create) +
			strlen(POOL_DESC) + 1;
	struct rpmem_msg_create *msg = MALLOC(msg_size);

	srv_accept(s);
	srv_recv(s, msg, msg_size);
	rpmem_ntoh_msg_create(msg);
	check_create_msg(msg);
	srv_send(s, resp, sizeof(*resp));
	srv_disconnect(s);

	FREE(msg);
}

/*
 * Number of cases for EPROTO test. Must be kept in sync with the
 * server_create_eproto function.
 */
#define CREATE_EPROTO_COUNT 8

/*
 * server_create_eproto -- send invalid create request responses to a client
 */
static void
server_create_eproto(struct server *s)
{
	for (int i = 0; i < CREATE_EPROTO_COUNT; i++) {
		struct rpmem_msg_create_resp resp = CREATE_RESP;

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
		case 5:
			resp.ibc.port = 0;
			break;
		case 6:
			resp.ibc.port = UINT16_MAX + 1;
			break;
		case 7:
			resp.ibc.persist_method = MAX_RPMEM_PM;
			break;
		default:
			UT_ASSERT(0);
			break;
		}

		rpmem_hton_msg_create_resp(&resp);

		server_create_handle(s, &resp);
	}
}

/*
 * server_create_error -- return an error status in create response message
 */
static void
server_create_error(struct server *s)
{
	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		struct rpmem_msg_create_resp resp = CREATE_RESP;
		resp.hdr.status = e;
		rpmem_hton_msg_create_resp(&resp);
		server_create_handle(s, &resp);
	}
}

/*
 * server_create -- test case for rpmem_obc_create function - server side
 */
void
server_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s 0|<port>", tc->name);

	unsigned short port = srv_get_port(argv[0]);

	struct server *s = srv_listen(port);

	struct rpmem_msg_create_resp resp = CREATE_RESP;
	rpmem_hton_msg_create_resp(&resp);

	server_econnreset(s, &resp, sizeof(resp) / 2);

	server_create_eproto(s);

	server_create_error(s);

	server_create_handle(s, &resp);

	srv_stop(s);
}

/*
 * client_create_errno -- perform create request operation and expect
 * specified errno, repeat the operation specified number of times.
 * If ex_errno is zero expect certain values in res struct.
 */
static void
client_create_errno(char *target, int ex_errno, int count)
{
	struct rpmem_req_attr req = {
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.pool_desc = POOL_DESC,
	};

	struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

	struct rpmem_resp_attr res;
	int ret;

	for (int i = 0; i < count; i++) {
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		client_connect_wait(rpc, target);

		ret = rpmem_obc_create(rpc, &req, &res, &pool_attr);
		if (ex_errno) {
			UT_ASSERTne(ret, 0);
			UT_ASSERTeq(errno, ex_errno);
		} else {
			UT_ASSERTeq(ret, 0);

			UT_ASSERTeq(res.port, CREATE_RESP.ibc.port);
			UT_ASSERTeq(res.rkey, CREATE_RESP.ibc.rkey);
			UT_ASSERTeq(res.raddr, CREATE_RESP.ibc.raddr);
			UT_ASSERTeq(res.persist_method,
					CREATE_RESP.ibc.persist_method);
			UT_ASSERTeq(res.nlanes,
					CREATE_RESP.ibc.nlanes);
		}

		rpmem_obc_disconnect(rpc);

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_create_error -- check if valid errno is set if error status returned
 */
static void
client_create_error(char *target)
{
	struct rpmem_req_attr req = {
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.pool_desc = POOL_DESC,
	};

	struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

	struct rpmem_resp_attr res;
	int ret;

	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		int ex_errno = rpmem_util_proto_errno(e);
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		client_connect_wait(rpc, target);

		ret = rpmem_obc_create(rpc, &req, &res, &pool_attr);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);

		rpmem_obc_disconnect(rpc);

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_create -- test case for create request operation - client side
 */
void
client_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < ECONNRESET_LOOP; i++)
		client_create_errno(target, ECONNRESET, ECONNRESET_COUNT);

	client_create_errno(target, EPROTO, CREATE_EPROTO_COUNT);

	client_create_error(target);

	client_create_errno(target, 0, 1);
}
