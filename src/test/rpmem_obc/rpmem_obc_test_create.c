// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

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
	UT_ASSERTeq(msg->c.major, RPMEM_PROTO_MAJOR);
	UT_ASSERTeq(msg->c.minor, RPMEM_PROTO_MINOR);
	UT_ASSERTeq(msg->c.pool_size, POOL_SIZE);
	UT_ASSERTeq(msg->c.provider, PROVIDER);
	UT_ASSERTeq(msg->c.nlanes, NLANES);
	UT_ASSERTeq(msg->c.buff_size, BUFF_SIZE);
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

	srv_recv(s, msg, msg_size);
	rpmem_ntoh_msg_create(msg);
	check_create_msg(msg);
	srv_send(s, resp, sizeof(*resp));

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
int
server_create_eproto(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, CREATE_EPROTO_COUNT - 1);

	int i = atoi(argv[0]);

	struct server *s = srv_init();

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

	srv_fini(s);

	return 1;
}

/*
 * server_create_error -- return an error status in create response message
 */
int
server_create_error(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, MAX_RPMEM_ERR);

	enum rpmem_err e = (enum rpmem_err)atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_create_resp resp = CREATE_RESP;
	resp.hdr.status = e;
	rpmem_hton_msg_create_resp(&resp);
	server_create_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * server_create_econnreset -- test case for closing connection - server side
 */
int
server_create_econnreset(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0|1", tc->name);

	int do_send = atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_create_resp resp = CREATE_RESP;
	rpmem_hton_msg_create_resp(&resp);

	if (do_send)
		srv_send(s, &resp, sizeof(resp) / 2);

	srv_fini(s);

	return 1;
}

/*
 * server_create -- test case for rpmem_obc_create function - server side
 */
int
server_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 0)
		UT_FATAL("usage: %s", tc->name);

	struct server *s = srv_init();

	struct rpmem_msg_create_resp resp = CREATE_RESP;
	rpmem_hton_msg_create_resp(&resp);

	server_create_handle(s, &resp);

	srv_fini(s);

	return 0;
}

/*
 * client_create_errno -- perform create request operation and expect
 * specified errno. If ex_errno is zero expect certain values in res struct.
 */
static void
client_create_errno(char *target, int ex_errno)
{
	struct rpmem_req_attr req = {
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.pool_desc = POOL_DESC,
		.buff_size = BUFF_SIZE,
	};

	struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

	struct rpmem_resp_attr res;
	int ret;

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
		.buff_size = BUFF_SIZE,
	};

	struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

	struct rpmem_resp_attr res;
	int ret;

	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		set_rpmem_cmd("server_create_error %d", e);

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
int
client_create(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < ECONNRESET_LOOP; i++) {
		set_rpmem_cmd("server_create_econnreset %d", i % 2);

		client_create_errno(target, ECONNRESET);
	}

	for (int i = 0; i < CREATE_EPROTO_COUNT; i++) {
		set_rpmem_cmd("server_create_eproto %d", i);

		client_create_errno(target, EPROTO);
	}

	client_create_error(target);

	set_rpmem_cmd("server_create");

	client_create_errno(target, 0);

	return 1;
}
