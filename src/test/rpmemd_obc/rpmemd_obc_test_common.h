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
#include "rpmemd_log.h"
#include "rpmemd_obc.h"

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

int clnt_connect(char *target);
int clnt_connect_wait(char *target);
void clnt_wait_disconnect(int fd);
void clnt_send(int fd, const void *buff, size_t len);
void clnt_recv(int fd, void *buff, size_t len);

static inline void
clnt_close(int fd)
{
	close(fd);
}

enum conn_wait_close {
	CONN_CLOSE,
	CONN_WAIT_CLOSE,
};

void server_bad_msg(struct rpmemd_obc *rpcd, int count);
void server_msg_resp(struct rpmemd_obc *rpc, enum rpmem_msg_type type,
		int status);
void server_msg_noresp(struct rpmemd_obc *rpc, enum rpmem_msg_type type);

extern struct rpmemd_obc_client_requests REQ_CB;

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

static const struct rpmem_msg_remove REMOVE_MSG = {
	.hdr = {
		.type = RPMEM_MSG_TYPE_REMOVE,
		.size = sizeof(struct rpmem_msg_remove),
	},
	.major = RPMEM_PROTO_MAJOR,
	.minor = RPMEM_PROTO_MINOR,
	.pool_desc = {
		.size = POOL_DESC_SIZE,
	},
};

TEST_CASE_DECLARE(server_accept_sim);
TEST_CASE_DECLARE(server_accept_sim_fork);
TEST_CASE_DECLARE(client_accept_sim);
TEST_CASE_DECLARE(server_accept_seq);
TEST_CASE_DECLARE(server_accept_seq_fork);
TEST_CASE_DECLARE(client_accept_seq);
TEST_CASE_DECLARE(client_bad_msg_hdr);
TEST_CASE_DECLARE(server_bad_msg_hdr);
TEST_CASE_DECLARE(client_econnreset);
TEST_CASE_DECLARE(server_econnreset);
TEST_CASE_DECLARE(server_create);
TEST_CASE_DECLARE(client_create);
TEST_CASE_DECLARE(server_open);
TEST_CASE_DECLARE(client_close);
TEST_CASE_DECLARE(server_close);
TEST_CASE_DECLARE(client_open);
TEST_CASE_DECLARE(server_remove);
TEST_CASE_DECLARE(client_remove);
