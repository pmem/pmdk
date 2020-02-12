// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmem_obc_test_common.h -- common declarations for rpmem_obc test
 */

#include "unittest.h"
#include "out.h"

#include "librpmem.h"
#include "rpmem.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "rpmem_obc.h"

#define POOL_SIZE	1024
#define NLANES		32
#define NLANES_RESP	16
#define PROVIDER	RPMEM_PROV_LIBFABRIC_SOCKETS
#define POOL_DESC	"pool_desc"
#define RKEY		0xabababababababab
#define RADDR		0x0101010101010101
#define PORT		1234
#define BUFF_SIZE	8192

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

static const struct rpmem_pool_attr POOL_ATTR = POOL_ATTR_INIT;

struct server {
	int fd_in;
	int fd_out;
};

void set_rpmem_cmd(const char *fmt, ...);

struct server *srv_init(void);
void srv_fini(struct server *s);
void srv_recv(struct server *s, void *buff, size_t len);
void srv_send(struct server *s, const void *buff, size_t len);
void srv_wait_disconnect(struct server *s);

void client_connect_wait(struct rpmem_obc *rpc, char *target);

/*
 * Since the server may disconnect the connection at any moment
 * from the client's perspective, execute the test in a loop so
 * the moment when the connection is closed will be possibly different.
 */
#define ECONNRESET_LOOP 10

void server_econnreset(struct server *s, const void *msg, size_t len);

TEST_CASE_DECLARE(client_enotconn);
TEST_CASE_DECLARE(client_connect);
TEST_CASE_DECLARE(client_monitor);
TEST_CASE_DECLARE(server_monitor);
TEST_CASE_DECLARE(server_wait);

TEST_CASE_DECLARE(client_create);
TEST_CASE_DECLARE(server_create);
TEST_CASE_DECLARE(server_create_econnreset);
TEST_CASE_DECLARE(server_create_eproto);
TEST_CASE_DECLARE(server_create_error);

TEST_CASE_DECLARE(client_open);
TEST_CASE_DECLARE(server_open);
TEST_CASE_DECLARE(server_open_econnreset);
TEST_CASE_DECLARE(server_open_eproto);
TEST_CASE_DECLARE(server_open_error);

TEST_CASE_DECLARE(client_close);
TEST_CASE_DECLARE(server_close);
TEST_CASE_DECLARE(server_close_econnreset);
TEST_CASE_DECLARE(server_close_eproto);
TEST_CASE_DECLARE(server_close_error);

TEST_CASE_DECLARE(client_set_attr);
TEST_CASE_DECLARE(server_set_attr);
TEST_CASE_DECLARE(server_set_attr_econnreset);
TEST_CASE_DECLARE(server_set_attr_eproto);
TEST_CASE_DECLARE(server_set_attr_error);
