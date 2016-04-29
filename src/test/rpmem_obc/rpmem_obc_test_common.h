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

static const struct rpmem_pool_attr POOL_ATTR = POOL_ATTR_INIT;

struct server {
	int fd;
	int cfd;
};

struct server *srv_listen(unsigned short port);
void srv_disconnect(struct server *s);
void srv_stop(struct server *s);
void srv_accept(struct server *s);
void srv_recv(struct server *s, void *buff, size_t len);
void srv_send(struct server *s, const void *buff, size_t len);
unsigned short srv_get_port(const char *str_port);
void srv_wait_disconnect(struct server *s);

void client_connect_wait(struct rpmem_obc *rpc, char *target);

/*
 * Since the server may disconnect the connection at any moment
 * from the client's perspective, execute the test in a loop so
 * the moment when the connection is closed will be possibly different.
 */
#define ECONNRESET_LOOP 1

/*
 * Number of cases for ECONNRESET. Must be kept in sync with the
 * server_create_econnreset function.
 */
#define ECONNRESET_COUNT 2

void server_econnreset(struct server *s, const void *msg, size_t len);

TEST_CASE_DECLARE(client_enotconn);
TEST_CASE_DECLARE(client_connect);
TEST_CASE_DECLARE(client_monitor);
TEST_CASE_DECLARE(server_monitor);
TEST_CASE_DECLARE(server_wait);
TEST_CASE_DECLARE(client_create);
TEST_CASE_DECLARE(server_create);
TEST_CASE_DECLARE(client_open);
TEST_CASE_DECLARE(server_open);
TEST_CASE_DECLARE(client_close);
TEST_CASE_DECLARE(server_close);
TEST_CASE_DECLARE(client_remove);
TEST_CASE_DECLARE(server_remove);
