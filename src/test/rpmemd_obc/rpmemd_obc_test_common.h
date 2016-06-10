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
 * rpmemd_obc_test_common.h -- common declarations for rpmemd_obc test
 */

#include "unittest.h"
#include "out.h"

#include "librpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_ssh.h"
#include "rpmemd_log.h"
#include "rpmemd_obc.h"
#include "base64.h"

#define PORT		1234
#define RKEY		0x0123456789abcdef
#define RADDR		0xfedcba9876543210
#define PERSIST_METHOD	RPMEM_PM_APM
#define SIGNATURE	"<RPMEM>"
#define MAJOR		1
#define COMPAT_F	2
#define INCOMPAT_F	3
#define ROCOMPAT_F	4
#define POOLSET_UUID	"POOLSET_UUID0123"
#define UUID		"UUID0123456789AB"
#define NEXT_UUID	"NEXT_UUID0123456"
#define PREV_UUID	"PREV_UUID0123456"
#define USER_FLAGS	"USER_FLAGS012345"
#define POOL_ATTR_INIT {\
	.signature = SIGNATURE,\
	.major = MAJOR,\
	.compat_features = COMPAT_F,\
	.incompat_features = INCOMPAT_F,\
	.ro_compat_features = ROCOMPAT_F,\
	.poolset_uuid = POOLSET_UUID,\
	.uuid = UUID,\
	.next_uuid = NEXT_UUID,\
	.prev_uuid = PREV_UUID,\
	.user_flags = USER_FLAGS,\
}
#define POOL_SIZE	0x0001234567abcdef
#define NLANES		0x123
#define NLANES_RESP	16
#define PROVIDER	RPMEM_PROV_LIBFABRIC_SOCKETS
#define POOL_DESC	"pool.set"

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
	.major = RPMEM_PROTO_MAJOR,
	.minor = RPMEM_PROTO_MINOR,
	.pool_size = POOL_SIZE,
	.nlanes = NLANES,
	.provider = PROVIDER,
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
	.major = RPMEM_PROTO_MAJOR,
	.minor = RPMEM_PROTO_MINOR,
	.pool_size = POOL_SIZE,
	.nlanes = NLANES,
	.provider = PROVIDER,
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
