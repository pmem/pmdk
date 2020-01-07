// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmem_obc_test_open.c -- test cases for rpmem_obj_open function
 */

#include "rpmem_obc_test_common.h"

static const struct rpmem_msg_open_resp OPEN_RESP = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_OPEN_RESP,
		.size = sizeof(struct rpmem_msg_open_resp),
		.status = 0,
	},
	.ibc	= {
		.port = PORT,
		.rkey = RKEY,
		.raddr = RADDR,
		.persist_method = RPMEM_PM_GPSPM,
		.nlanes = NLANES_RESP,
	},
	.pool_attr = POOL_ATTR_INIT,
};

/*
 * check_open_msg -- check open message
 */
static void
check_open_msg(struct rpmem_msg_open *msg)
{
	size_t pool_desc_size = strlen(POOL_DESC) + 1;
	size_t msg_size = sizeof(struct rpmem_msg_open) + pool_desc_size;

	UT_ASSERTeq(msg->hdr.type, RPMEM_MSG_TYPE_OPEN);
	UT_ASSERTeq(msg->hdr.size, msg_size);
	UT_ASSERTeq(msg->c.major, RPMEM_PROTO_MAJOR);
	UT_ASSERTeq(msg->c.minor, RPMEM_PROTO_MINOR);
	UT_ASSERTeq(msg->c.pool_size, POOL_SIZE);
	UT_ASSERTeq(msg->c.provider, PROVIDER);
	UT_ASSERTeq(msg->c.nlanes, NLANES);
	UT_ASSERTeq(msg->c.buff_size, BUFF_SIZE);
	UT_ASSERTeq(msg->pool_desc.size, pool_desc_size);
	UT_ASSERTeq(strcmp((char *)msg->pool_desc.desc, POOL_DESC), 0);
}

/*
 * server_open_handle -- handle an open request message
 */
static void
server_open_handle(struct server *s, const struct rpmem_msg_open_resp *resp)
{
	size_t msg_size = sizeof(struct rpmem_msg_open) +
			strlen(POOL_DESC) + 1;
	struct rpmem_msg_open *msg = MALLOC(msg_size);

	srv_recv(s, msg, msg_size);
	rpmem_ntoh_msg_open(msg);
	check_open_msg(msg);
	srv_send(s, resp, sizeof(*resp));

	FREE(msg);
}

/*
 * Number of cases for EPROTO test. Must be kept in sync with the
 * server_open_eproto function.
 */
#define OPEN_EPROTO_COUNT 8

/*
 * server_open_eproto -- send invalid open request responses to a client
 */
int
server_open_eproto(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, OPEN_EPROTO_COUNT - 1);

	int i = atoi(argv[0]);

	struct server *s = srv_init();
	struct rpmem_msg_open_resp resp = OPEN_RESP;

	switch (i) {
	case 0:
		resp.hdr.type = MAX_RPMEM_MSG_TYPE;
		break;
	case 1:
		resp.hdr.type = RPMEM_MSG_TYPE_CREATE_RESP;
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

	rpmem_hton_msg_open_resp(&resp);

	server_open_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * server_open_error -- return error status in open response message
 */
int
server_open_error(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, MAX_RPMEM_ERR);

	enum rpmem_err e = (enum rpmem_err)atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_open_resp resp = OPEN_RESP;
	resp.hdr.status = e;
	rpmem_hton_msg_open_resp(&resp);
	server_open_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * server_open -- test case for rpmem_obc_create function - server side
 */
int
server_open_econnreset(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0|1", tc->name);

	int do_send = atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_open_resp resp = OPEN_RESP;
	rpmem_hton_msg_open_resp(&resp);

	if (do_send)
		srv_send(s, &resp, sizeof(resp) / 2);

	srv_fini(s);

	return 1;
}

/*
 * server_open -- test case for open request message - server side
 */
int
server_open(const struct test_case *tc, int argc, char *argv[])
{
	struct server *s = srv_init();

	struct rpmem_msg_open_resp resp = OPEN_RESP;
	rpmem_hton_msg_open_resp(&resp);

	server_open_handle(s, &resp);

	srv_fini(s);

	return 0;
}

/*
 * client_open_errno -- perform open request operation and expect
 * specified errno, repeat the operation specified number of times.
 * If ex_errno is zero expect certain values in res struct.
 */
static void
client_open_errno(char *target, int ex_errno)
{
	struct rpmem_req_attr req = {
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.pool_desc = POOL_DESC,
		.buff_size = BUFF_SIZE,
	};

	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	struct rpmem_resp_attr res;
	int ret;

	struct rpmem_obc *rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	client_connect_wait(rpc, target);

	ret = rpmem_obc_open(rpc, &req, &res, &pool_attr);
	if (ex_errno) {
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);
	} else {
		UT_ASSERTeq(ret, 0);

		UT_ASSERTeq(res.port, OPEN_RESP.ibc.port);
		UT_ASSERTeq(res.rkey, OPEN_RESP.ibc.rkey);
		UT_ASSERTeq(res.raddr, OPEN_RESP.ibc.raddr);
		UT_ASSERTeq(res.persist_method,
				OPEN_RESP.ibc.persist_method);
		UT_ASSERTeq(res.nlanes,
				OPEN_RESP.ibc.nlanes);

		UT_ASSERTeq(memcmp(pool_attr.signature,
				OPEN_RESP.pool_attr.signature,
				RPMEM_POOL_HDR_SIG_LEN), 0);
		UT_ASSERTeq(pool_attr.major, OPEN_RESP.pool_attr.major);
		UT_ASSERTeq(pool_attr.compat_features,
				OPEN_RESP.pool_attr.compat_features);
		UT_ASSERTeq(pool_attr.incompat_features,
				OPEN_RESP.pool_attr.incompat_features);
		UT_ASSERTeq(pool_attr.ro_compat_features,
				OPEN_RESP.pool_attr.ro_compat_features);
		UT_ASSERTeq(memcmp(pool_attr.poolset_uuid,
				OPEN_RESP.pool_attr.poolset_uuid,
				RPMEM_POOL_HDR_UUID_LEN), 0);
		UT_ASSERTeq(memcmp(pool_attr.uuid,
				OPEN_RESP.pool_attr.uuid,
				RPMEM_POOL_HDR_UUID_LEN), 0);
		UT_ASSERTeq(memcmp(pool_attr.next_uuid,
				OPEN_RESP.pool_attr.next_uuid,
				RPMEM_POOL_HDR_UUID_LEN), 0);
		UT_ASSERTeq(memcmp(pool_attr.prev_uuid,
				OPEN_RESP.pool_attr.prev_uuid,
				RPMEM_POOL_HDR_UUID_LEN), 0);
		UT_ASSERTeq(memcmp(pool_attr.user_flags,
				OPEN_RESP.pool_attr.user_flags,
				RPMEM_POOL_USER_FLAGS_LEN), 0);
	}

	rpmem_obc_disconnect(rpc);

	rpmem_obc_fini(rpc);
}

/*
 * client_open_error -- check if valid errno is set if error status returned
 */
static void
client_open_error(char *target)
{
	struct rpmem_req_attr req = {
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.pool_desc = POOL_DESC,
		.buff_size = BUFF_SIZE,
	};

	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	struct rpmem_resp_attr res;
	int ret;

	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		set_rpmem_cmd("server_open_error %d", e);

		int ex_errno = rpmem_util_proto_errno(e);
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		client_connect_wait(rpc, target);

		ret = rpmem_obc_open(rpc, &req, &res, &pool_attr);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);

		rpmem_obc_disconnect(rpc);

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_open -- test case for open request message - client side
 */
int
client_open(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < ECONNRESET_LOOP; i++) {
		set_rpmem_cmd("server_open_econnreset %d", i % 2);

		client_open_errno(target, ECONNRESET);
	}

	for (int i = 0; i < OPEN_EPROTO_COUNT; i++) {
		set_rpmem_cmd("server_open_eproto %d", i);

		client_open_errno(target, EPROTO);
	}

	client_open_error(target);

	set_rpmem_cmd("server_open");

	client_open_errno(target, 0);

	return 1;
}
