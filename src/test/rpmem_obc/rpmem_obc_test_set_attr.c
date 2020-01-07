// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2017, Intel Corporation */

/*
 * rpmem_obc_test_set_attr.c -- test cases for rpmem_set_attr function
 */

#include "rpmem_obc_test_common.h"

static const struct rpmem_msg_set_attr_resp SET_ATTR_RESP = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_SET_ATTR_RESP,
		.size = sizeof(struct rpmem_msg_set_attr_resp),
		.status = 0,
	}
};

/*
 * check_set_attr_msg -- check set attributes message
 */
static void
check_set_attr_msg(struct rpmem_msg_set_attr *msg)
{
	size_t msg_size = sizeof(struct rpmem_msg_set_attr);
	struct rpmem_pool_attr pool_attr = POOL_ATTR_ALT;

	UT_ASSERTeq(msg->hdr.type, RPMEM_MSG_TYPE_SET_ATTR);
	UT_ASSERTeq(msg->hdr.size, msg_size);
	UT_ASSERTeq(memcmp(&msg->pool_attr, &pool_attr, sizeof(pool_attr)), 0);
}

/*
 * server_open_handle -- handle an set attributes request message
 */
static void
server_set_attr_handle(struct server *s,
	const struct rpmem_msg_set_attr_resp *resp)
{
	size_t msg_size = sizeof(struct rpmem_msg_set_attr);
	struct rpmem_msg_set_attr *msg = MALLOC(msg_size);

	srv_recv(s, msg, msg_size);
	rpmem_ntoh_msg_set_attr(msg);
	check_set_attr_msg(msg);
	srv_send(s, resp, sizeof(*resp));

	FREE(msg);
}

/*
 * Number of cases for EPROTO test. Must be kept in sync with the
 * server_set_attr_eproto function.
 */
#define SET_ATTR_EPROTO_COUNT 5

/*
 * server_set_attr_eproto -- send invalid set attributes request responses to
 * a client
 */
int
server_set_attr_eproto(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, SET_ATTR_EPROTO_COUNT - 1);

	int i = atoi(argv[0]);

	struct server *s = srv_init();
	struct rpmem_msg_set_attr_resp resp = SET_ATTR_RESP;

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
	default:
		UT_ASSERT(0);
		break;
	}

	rpmem_hton_msg_set_attr_resp(&resp);

	server_set_attr_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * server_set_attr_error -- return error status in set attributes response
 * message
 */
int
server_set_attr_error(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0-%d", tc->name, MAX_RPMEM_ERR);

	enum rpmem_err e = (enum rpmem_err)atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_set_attr_resp resp = SET_ATTR_RESP;
	resp.hdr.status = e;
	rpmem_hton_msg_set_attr_resp(&resp);
	server_set_attr_handle(s, &resp);

	srv_fini(s);

	return 1;
}

/*
 * server_set_attr_econnreset -- test case for closing connection - server side
 */
int
server_set_attr_econnreset(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s 0|1", tc->name);

	int do_send = atoi(argv[0]);

	struct server *s = srv_init();

	struct rpmem_msg_set_attr_resp resp = SET_ATTR_RESP;
	rpmem_hton_msg_set_attr_resp(&resp);

	if (do_send)
		srv_send(s, &resp, sizeof(resp) / 2);

	srv_fini(s);

	return 1;
}

/*
 * server_set_attr -- test case for rpmem_obc_set_attr - server side
 * side
 */
int
server_set_attr(const struct test_case *tc, int argc, char *argv[])
{
	struct server *s = srv_init();

	struct rpmem_msg_set_attr_resp resp = SET_ATTR_RESP;
	rpmem_hton_msg_set_attr_resp(&resp);

	server_set_attr_handle(s, &resp);

	srv_fini(s);

	return 0;
}

/*
 * client_set_attr_init -- initialize communication - client side
 */
static struct rpmem_obc *
client_set_attr_init(char *target)
{
	struct rpmem_pool_attr pool_attr;
	memset(&pool_attr, 0, sizeof(pool_attr));

	struct rpmem_obc *rpc = rpmem_obc_init();
	UT_ASSERTne(rpc, NULL);

	client_connect_wait(rpc, target);

	return rpc;
}

/*
 * client_set_attr_fini -- finalize communication - client side
 */
static void
client_set_attr_fini(struct rpmem_obc *rpc)
{
	rpmem_obc_disconnect(rpc);

	rpmem_obc_fini(rpc);
}

/*
 * client_set_attr_errno -- perform set attributes request operation and expect
 * specified errno.
 */
static void
client_set_attr_errno(char *target, int ex_errno)
{
	struct rpmem_obc *rpc = client_set_attr_init(target);

	const struct rpmem_pool_attr pool_attr_alt = POOL_ATTR_ALT;

	int ret = rpmem_obc_set_attr(rpc, &pool_attr_alt);

	if (ex_errno) {
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);
	} else {
		UT_ASSERTeq(ret, 0);
	}

	client_set_attr_fini(rpc);
}

/*
 * client_set_attr_error -- check if valid errno is set if error status
 * returned
 */
static void
client_set_attr_error(char *target)
{
	int ret;

	for (enum rpmem_err e = 1; e < MAX_RPMEM_ERR; e++) {
		set_rpmem_cmd("server_set_attr_error %d", e);

		int ex_errno = rpmem_util_proto_errno(e);
		struct rpmem_obc *rpc = client_set_attr_init(target);

		const struct rpmem_pool_attr pool_attr_alt = POOL_ATTR_ALT;

		ret = rpmem_obc_set_attr(rpc, &pool_attr_alt);
		UT_ASSERTne(ret, 0);
		UT_ASSERTeq(errno, ex_errno);

		client_set_attr_fini(rpc);
	}
}
/*
 * client_set_attr -- test case for set attributes request operation - client
 * side
 */
int
client_set_attr(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]", tc->name);

	char *target = argv[0];

	for (int i = 0; i < ECONNRESET_LOOP; i++) {
		set_rpmem_cmd("server_set_attr_econnreset %d", i % 2);

		client_set_attr_errno(target, ECONNRESET);
	}

	for (int i = 0; i < SET_ATTR_EPROTO_COUNT; i++) {
		set_rpmem_cmd("server_set_attr_eproto %d", i);

		client_set_attr_errno(target, EPROTO);
	}

	client_set_attr_error(target);

	set_rpmem_cmd("server_set_attr");

	client_set_attr_errno(target, 0);

	return 1;
}
