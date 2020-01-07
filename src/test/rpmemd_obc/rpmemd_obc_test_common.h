// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmemd_obc_test_common.h -- common declarations for rpmemd_obc test
 */

#include "unittest.h"

#include "librpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_ssh.h"
#include "rpmem_util.h"
#include "rpmemd_log.h"
#include "rpmemd_obc.h"

#define PORT		1234
#define RKEY		0x0123456789abcdef
#define RADDR		0xfedcba9876543210
#define PERSIST_METHOD	RPMEM_PM_APM
#define POOL_ATTR_INIT {\
	.signature		= "<RPMEM>",\
	.major			= 1,\
	.compat_features	= 2,\
	.incompat_features	= 3,\
	.ro_compat_features	= 4,\
	.poolset_uuid		= "POOLSET_UUID0123",\
	.uuid			= "UUID0123456789AB",\
	.next_uuid		= "NEXT_UUID0123456",\
	.prev_uuid		= "PREV_UUID0123456",\
	.user_flags		= "USER_FLAGS012345",\
}
#define POOL_ATTR_ALT {\
	.signature		= "<ALT>",\
	.major			= 5,\
	.compat_features	= 6,\
	.incompat_features	= 7,\
	.ro_compat_features	= 8,\
	.poolset_uuid		= "UUID_POOLSET_ALT",\
	.uuid			= "ALT_UUIDCDEFFEDC",\
	.next_uuid		= "456UUID_NEXT_ALT",\
	.prev_uuid		= "UUID012_ALT_PREV",\
	.user_flags		= "012345USER_FLAGS",\
}
#define POOL_SIZE	0x0001234567abcdef
#define NLANES		0x123
#define NLANES_RESP	16
#define PROVIDER	RPMEM_PROV_LIBFABRIC_SOCKETS
#define POOL_DESC	"pool.set"
#define BUFF_SIZE	8192

static const char pool_desc[] = POOL_DESC;
#define POOL_DESC_SIZE	(sizeof(pool_desc) / sizeof(pool_desc[0]))

struct rpmem_ssh *clnt_connect(char *target);
void clnt_wait_disconnect(struct rpmem_ssh *ssh);
void clnt_send(struct rpmem_ssh *ssh, const void *buff, size_t len);
void clnt_recv(struct rpmem_ssh *ssh, void *buff, size_t len);
void clnt_close(struct rpmem_ssh *ssh);

enum conn_wait_close {
	CONN_CLOSE,
	CONN_WAIT_CLOSE,
};

void set_rpmem_cmd(const char *fmt, ...);

extern struct rpmemd_obc_requests REQ_CB;

struct req_cb_arg {
	int resp;
	unsigned long long types;
	int force_ret;
	int ret;
	int status;
};

static const struct rpmem_msg_hdr MSG_HDR = {
	.type = RPMEM_MSG_TYPE_CLOSE,
	.size = sizeof(struct rpmem_msg_hdr),
};

static const struct rpmem_msg_create CREATE_MSG = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_CREATE,
		.size = sizeof(struct rpmem_msg_create),
	},
	.c = {
		.major = RPMEM_PROTO_MAJOR,
		.minor = RPMEM_PROTO_MINOR,
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.buff_size = BUFF_SIZE,
	},
	.pool_attr = POOL_ATTR_INIT,
	.pool_desc = {
		.size = POOL_DESC_SIZE,
	},
};

static const struct rpmem_msg_open OPEN_MSG = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_OPEN,
		.size = sizeof(struct rpmem_msg_open),
	},
	.c = {
		.major = RPMEM_PROTO_MAJOR,
		.minor = RPMEM_PROTO_MINOR,
		.pool_size = POOL_SIZE,
		.nlanes = NLANES,
		.provider = PROVIDER,
		.buff_size = BUFF_SIZE,
	},
	.pool_desc = {
		.size = POOL_DESC_SIZE,
	},
};

static const struct rpmem_msg_close CLOSE_MSG = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_CLOSE,
		.size = sizeof(struct rpmem_msg_close),
	},
};

static const struct rpmem_msg_set_attr SET_ATTR_MSG = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_SET_ATTR,
		.size = sizeof(struct rpmem_msg_set_attr),
	},
	.pool_attr = POOL_ATTR_ALT,
};

TEST_CASE_DECLARE(server_accept_sim);
TEST_CASE_DECLARE(server_accept_sim_fork);
TEST_CASE_DECLARE(client_accept_sim);
TEST_CASE_DECLARE(server_accept_seq);
TEST_CASE_DECLARE(server_accept_seq_fork);
TEST_CASE_DECLARE(client_accept_seq);

TEST_CASE_DECLARE(client_bad_msg_hdr);
TEST_CASE_DECLARE(server_bad_msg);
TEST_CASE_DECLARE(server_msg_noresp);
TEST_CASE_DECLARE(server_msg_resp);

TEST_CASE_DECLARE(client_econnreset);
TEST_CASE_DECLARE(server_econnreset);

TEST_CASE_DECLARE(client_create);
TEST_CASE_DECLARE(server_open);
TEST_CASE_DECLARE(client_close);
TEST_CASE_DECLARE(server_close);
TEST_CASE_DECLARE(client_open);

TEST_CASE_DECLARE(client_set_attr);
