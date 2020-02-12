// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmem_obc_test_misc.c -- miscellaneous test cases for rpmem_obc module
 */
#include <netdb.h>

#include "rpmem_obc_test_common.h"

static const struct rpmem_msg_close_resp CLOSE_RESP = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_CLOSE_RESP,
		.size = sizeof(struct rpmem_msg_close_resp),
		.status = 0,
	},
};

/*
 * client_enotconn -- check if ENOTCONN error is returned after
 * calling rpmem_obc API without connecting to the server.
 */
int
client_enotconn(const struct test_case *tc, int argc, char *argv[])
{
	struct rpmem_obc *rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	struct rpmem_req_attr req = {
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.pool_desc = POOL_DESC,
	};

	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));
	struct rpmem_resp_attr res;

	int ret;

	ret = rpmem_obc_monitor(rpc, 1);
	UT_ASSERTeq(ret, 0);

	ret = rpmem_obc_create(rpc, &req, &res, &pool_attr);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	ret = rpmem_obc_open(rpc, &req, &res, &pool_attr);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	ret = rpmem_obc_close(rpc, 0);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	ret = rpmem_obc_disconnect(rpc);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	rpmem_obc_fini(rpc);

	return 0;
}

/*
 * client_connect -- try to connect to the server at specified address and port
 */
int
client_connect(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]...", tc->name);

	for (int i = 0; i < argc; i++) {
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		struct rpmem_target_info *info;
		info = rpmem_target_parse(argv[i]);
		UT_ASSERTne(info, NULL);

		int ret = rpmem_obc_connect(rpc, info);
		if (ret) {
			UT_OUT("not connected: %s: %s", argv[i],
					out_get_errormsg());
		} else {
			UT_OUT("    connected: %s", argv[i]);
			rpmem_obc_disconnect(rpc);
		}

		rpmem_target_free(info);
		rpmem_obc_fini(rpc);
	}

	return argc;
}

/*
 * server_monitor -- test case for rpmem_obc_create function - server side
 */
int
server_monitor(const struct test_case *tc, int argc, char *argv[])
{
	struct server *s = srv_init();
	struct rpmem_msg_close close;
	struct rpmem_msg_close_resp resp = CLOSE_RESP;
	rpmem_hton_msg_close_resp(&resp);

	srv_recv(s, &close, sizeof(close));
	srv_send(s, &resp, sizeof(resp));

	srv_fini(s);

	return 0;
}

/*
 * server_monitor -- test case for rpmem_obc_monitor function - server side
 */
int
client_monitor(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	set_rpmem_cmd("server_monitor");

	{

		/*
		 * Connect to target node, check connection state before
		 * and after disconnecting.
		 */
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		struct rpmem_target_info *info;
		info = rpmem_target_parse(target);
		UT_ASSERTne(info, NULL);

		int ret = rpmem_obc_connect(rpc, info);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTeq(ret, 1);

		ret = rpmem_obc_disconnect(rpc);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTne(ret, 1);

		rpmem_target_free(info);
		rpmem_obc_fini(rpc);
	}

	{
		/*
		 * Connect to target node and expect that server will
		 * disconnect.
		 */
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		struct rpmem_target_info *info;
		info = rpmem_target_parse(target);
		UT_ASSERTne(info, NULL);

		int ret = rpmem_obc_connect(rpc, info);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTeq(ret, 1);

		ret = rpmem_obc_close(rpc, 0);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 0);
		UT_ASSERTne(ret, 1);

		rpmem_obc_disconnect(rpc);

		rpmem_target_free(info);
		rpmem_obc_fini(rpc);
	}

	return 1;
}
