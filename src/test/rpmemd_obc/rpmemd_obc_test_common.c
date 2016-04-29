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
 * rpmemd_obc_test_common.c -- common definitions for rpmemd_obc tests
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "rpmemd_obc_test_common.h"

/*
 * req_cb_check_req -- validate request attributes
 */
static void
req_cb_check_req(const struct rpmem_req_attr *req)
{
	UT_ASSERTeq(req->nlanes, NLANES);
	UT_ASSERTeq(req->pool_size, POOL_SIZE);
	UT_ASSERTeq(req->provider, PROVIDER);
	UT_ASSERTeq(strcmp(req->pool_desc, POOL_DESC), 0);

}

/*
 * req_cb_check_pool_attr -- validate pool attributes
 */
static void
req_cb_check_pool_attr(const struct rpmem_pool_attr *pool_attr)
{
	struct rpmem_pool_attr attr = POOL_ATTR_INIT;
	UT_ASSERTeq(memcmp(&attr, pool_attr, sizeof(attr)), 0);
}

/*
 * req_cb_create -- callback for create request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_create(struct rpmemd_obc_client *client, void *arg,
	const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr)
{
	UT_ASSERTne(arg, NULL);
	UT_ASSERTne(req, NULL);
	UT_ASSERTne(pool_attr, NULL);

	req_cb_check_req(req);
	req_cb_check_pool_attr(pool_attr);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_CREATE);

	int ret = args->ret;

	if (args->resp) {
		struct rpmem_resp_attr resp = {
			.port = PORT,
			.rkey = RKEY,
			.raddr = RADDR,
			.persist_method = PERSIST_METHOD,
			.nlanes = NLANES_RESP,
		};

		ret = rpmemd_obc_client_create_resp(client,
				args->status, &resp);
	}

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * req_cb_open -- callback for open request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_open(struct rpmemd_obc_client *client, void *arg,
	const struct rpmem_req_attr *req)
{
	UT_ASSERTne(arg, NULL);
	UT_ASSERTne(req, NULL);

	req_cb_check_req(req);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_OPEN);

	int ret = args->ret;

	if (args->resp) {
		struct rpmem_resp_attr resp = {
			.port = PORT,
			.rkey = RKEY,
			.raddr = RADDR,
			.persist_method = PERSIST_METHOD,
			.nlanes = NLANES_RESP,
		};

		struct rpmem_pool_attr pool_attr = POOL_ATTR_INIT;

		ret = rpmemd_obc_client_open_resp(client, args->status,
				&resp, &pool_attr);
	}

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * req_cb_close -- callback for close request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_close(struct rpmemd_obc_client *client, void *arg)
{
	UT_ASSERTne(arg, NULL);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_CLOSE);

	int ret = args->ret;

	if (args->resp)
		ret = rpmemd_obc_client_close_resp(client, args->status);

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * req_cb_remove -- callback for remove request operation
 *
 * This function behaves according to arguments specified via
 * struct req_cb_arg.
 */
static int
req_cb_remove(struct rpmemd_obc_client *client, void *arg,
		const char *pool_desc)
{
	UT_ASSERTne(arg, NULL);
	UT_ASSERTne(pool_desc, NULL);
	UT_ASSERTeq(strcmp(pool_desc, POOL_DESC), 0);

	struct req_cb_arg *args = arg;

	args->types |= (1 << RPMEM_MSG_TYPE_REMOVE);

	int ret = args->ret;

	if (args->resp)
		ret = rpmemd_obc_client_remove_resp(client, args->status);

	if (args->force_ret)
		ret = args->ret;

	return ret;
}

/*
 * REQ_CB -- request callbacks
 */
struct rpmemd_obc_client_requests REQ_CB = {
	.create = req_cb_create,
	.open = req_cb_open,
	.close = req_cb_close,
	.remove = req_cb_remove,
};

/*
 * clnt_connect_wait -- connect with target in a loop
 */
int
clnt_connect_wait(char *target)
{
	int fd;
	do {
		fd = clnt_connect(target);
	} while (fd == -1);

	return fd;
}

/*
 * clnt_wait_disconnect -- wait for disconnection
 */
void
clnt_wait_disconnect(int fd)
{
	int buff;
	ssize_t rret = read(fd, &buff, sizeof(buff));
	UT_ASSERT(rret <= 0);
}

/*
 * clnt_connect -- create a socket connection with specified target
 */
int
clnt_connect(char *target)
{
	char *node = STRDUP(target);
	char *service = strrchr(node, ':');
	if (service) {
		*service = '\0';
		service++;
	} else {
		service = RPMEM_SERVICE;
	}

	struct addrinfo *addrinfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	int ret = getaddrinfo(node, service, &hints, &addrinfo);
	UT_ASSERTeq(ret, 0);
	int sockfd;

	for (struct addrinfo *ai = addrinfo; ai; ai = ai->ai_next) {
		sockfd = socket(ai->ai_family, ai->ai_socktype,
				ai->ai_protocol);

		if (sockfd == -1)
			continue;

		if (!connect(sockfd, ai->ai_addr, ai->ai_addrlen))
			break;

		close(sockfd);
		sockfd = -1;
	}

	FREE(node);
	freeaddrinfo(addrinfo);

	return sockfd;
}

/*
 * clnt_send -- send data via socket
 */
void
clnt_send(int fd, const void *buff, size_t len)
{
	size_t wr = 0;
	const uint8_t *cbuf = buff;
	while (wr < len) {
		ssize_t ret = write(fd, &cbuf[wr], len - wr);
		UT_ASSERT(ret > 0);
		wr += (size_t)ret;
	}
}

/*
 * clnt_recv -- receive data via socket
 */
void
clnt_recv(int fd, void *buff, size_t len)
{
	size_t rd  = 0;
	uint8_t *cbuf = buff;
	while (rd < len) {
		ssize_t ret = read(fd, &cbuf[rd], len - rd);
		UT_ASSERT(ret > 0);
		rd += (size_t)ret;
	}
}

/*
 * server_bad_msg -- process a message specified number of times and expect
 * error returned from rpmemd_obc_client_process function
 */
void
server_bad_msg(struct rpmemd_obc *rpdc, int count)
{
	struct rpmemd_obc_client *client;
	int ret;

	for (int i = 0; i < count; i++) {
		client = rpmemd_obc_accept(rpdc);
		UT_ASSERTne(client, NULL);

		ret = rpmemd_obc_client_process(client, &REQ_CB, NULL);
		UT_ASSERTne(ret, 0);

		ret = rpmemd_obc_client_close(client);
		UT_ASSERTeq(ret, 0);

		rpmemd_obc_client_fini(client);
	}
}

/*
 * server_msg_args -- process a message according to specified arguments
 */
static void
server_msg_args(struct rpmemd_obc *rpdc, enum conn_wait_close conn,
	struct req_cb_arg *args)
{
	struct rpmemd_obc_client *client;
	int ret;
	unsigned long long types = args->types;
	args->types = 0;

	client = rpmemd_obc_accept(rpdc);
	UT_ASSERTne(client, NULL);

	ret = rpmemd_obc_client_process(client, &REQ_CB, args);
	UT_ASSERTeq(ret, args->ret);
	UT_ASSERTeq(args->types, types);

	if (conn == CONN_WAIT_CLOSE) {
		ret = rpmemd_obc_client_process(client, &REQ_CB, args);
		UT_ASSERTeq(ret, 1);
	}

	ret = rpmemd_obc_client_close(client);
	UT_ASSERTeq(ret, 0);

	rpmemd_obc_client_fini(client);
}

/*
 * server_msg_resp -- process a message of specified type, response to client
 * with specific status value and return status of sending response function
 */
void
server_msg_resp(struct rpmemd_obc *rpdc, enum rpmem_msg_type type, int status)
{
	struct req_cb_arg args = {
		.ret = 0,
		.force_ret = 0,
		.resp = 1,
		.types = (1 << type),
		.status = status,
	};

	server_msg_args(rpdc, CONN_WAIT_CLOSE, &args);
}

/*
 * server_msg_noresp -- process a message of specified type, do not response to
 * client and return specific value from process callback
 */
void
server_msg_noresp(struct rpmemd_obc *rpdc, enum rpmem_msg_type type)
{
	struct req_cb_arg args = {
		.ret = -1,
		.force_ret = 1,
		.resp = 0,
		.types = (1 << type),
		.status = 0,
	};

	server_msg_args(rpdc, CONN_CLOSE, &args);
}
