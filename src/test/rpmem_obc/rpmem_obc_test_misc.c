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
 * rpmem_obc_test_misc.c -- miscellaneous test cases for rpmem_obc module
 */
#include <netdb.h>

#include "rpmem_obc_test_common.h"

/*
 * client_enotconn -- check if ENOTCONN error is returned after
 * calling rpmem_obc API without connecting to the server.
 */
void
client_enotconn(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 0)
		UT_FATAL("usage: %s", tc->name);

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

	ret = rpmem_obc_remove(rpc, POOL_DESC);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	ret = rpmem_obc_close(rpc);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	ret = rpmem_obc_disconnect(rpc);
	UT_ASSERTne(ret, 0);
	UT_ASSERTeq(errno, ENOTCONN);

	rpmem_obc_fini(rpc);
}

/*
 * server_wait -- accept a connection from a client and wait until one
 * disconnects
 */
void
server_wait(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s 0|<port>", tc->name);

	unsigned short port = srv_get_port(argv[0]);

	struct server *s = srv_listen(port);
	srv_accept(s);
	srv_wait_disconnect(s);
	srv_stop(s);
}

/*
 * client_connect -- try to connect to the server at specified address and port
 */
void
client_connect(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]...", tc->name);

	for (int i = 0; i < argc; i++) {
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		int ret = rpmem_obc_connect(rpc, argv[i]);
		if (ret) {
			UT_OUT("not connected: %s: %s", argv[i],
					out_get_errormsg());
		} else {
			UT_OUT("    connected: %s", argv[i]);
			rpmem_obc_disconnect(rpc);
		}

		rpmem_obc_fini(rpc);
	}
}

/*
 * client_ctrl_connect -- establish auxiliary socket connection
 */
static int
client_ctrl_connect(char *target)
{
	char *node = STRDUP(target);
	char *service;
	char *colon = strrchr(node, ':');
	if (colon) {
		service = colon + 1;
		*colon = '\0';
	} else {
		service = RPMEM_SERVICE;
	}

	int sockfd = -1;
	struct addrinfo *addrinfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	int ret = getaddrinfo(node, service, &hints, &addrinfo);
	UT_ASSERTeq(ret, 0);

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

	freeaddrinfo(addrinfo);

	FREE(node);

	return sockfd;
}

/*
 * client_monitor -- test case for rpmem_obc_monitor function - client side
 */
void
client_monitor(const struct test_case *tc, int argc, char *argv[])
{
	if (argc < 1)
		UT_FATAL("usage: %s <addr>[:<port>]...", tc->name);

	char *target = argv[0];

	int ctrl = client_ctrl_connect(target);
	int buff = 0;
	UT_ASSERTne(ctrl, -1);

	{
		/*
		 * Connect to target node, check connection state before
		 * and after disconnecting.
		 */
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		int ret = rpmem_obc_connect(rpc, target);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTeq(ret, 1);

		ret = rpmem_obc_disconnect(rpc);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTne(ret, 1);

		rpmem_obc_fini(rpc);
	}

	{
		/*
		 * Connect to target node and expect that server will
		 * disconnect.
		 */
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		int ret = rpmem_obc_connect(rpc, target);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTeq(ret, 1);

		ssize_t wr = write(ctrl, &buff, sizeof(buff));
		UT_ASSERTeq(wr, sizeof(buff));

		ret = rpmem_obc_monitor(rpc, 0);
		UT_ASSERTne(ret, 1);

		rpmem_obc_fini(rpc);
	}

	{
		/*
		 * Connect to target node and expect that server will
		 * disconnect.
		 */
		struct rpmem_obc *rpc = rpmem_obc_init();
		UT_ASSERTne(rpc, NULL);

		int ret = rpmem_obc_connect(rpc, target);
		UT_ASSERTeq(ret, 0);

		ret = rpmem_obc_monitor(rpc, 1);
		UT_ASSERTeq(ret, 1);

		ssize_t wr = write(ctrl, &buff, sizeof(buff));
		UT_ASSERTeq(wr, sizeof(buff));

		ret = rpmem_obc_monitor(rpc, 0);
		UT_ASSERTne(ret, 1);

		rpmem_obc_fini(rpc);
	}


	close(ctrl);
}

/*
 * server_monitor -- test case for rpmem_obc_monitor function - server side
 */
void
server_monitor(const struct test_case *tc, int argc, char *argv[])
{
	if (argc != 1)
		UT_FATAL("usage: %s 0|<port>", tc->name);

	unsigned short port = srv_get_port(argv[0]);
	struct server *s = srv_listen(port);
	/*
	 * First connection is a control connection for
	 * synchronization of client and server.
	 */
	srv_accept(s);
	int ctrl = s->cfd;
	int buff = 0;

	{
		/*
		 * Accept a connection and wait for the client to disconnect.
		 */
		srv_accept(s);
		srv_wait_disconnect(s);
	}

	{
		/*
		 * Accept a connection and wait for the signal on ctrl
		 * connection from client and then disconnect.
		 */
		srv_accept(s);

		ssize_t rd = read(ctrl, &buff, sizeof(buff));
		UT_ASSERTeq(rd, sizeof(buff));

		srv_disconnect(s);

	}

	{
		/*
		 * Accept a connection and wait for the signal on ctrl
		 * connection from client and then send some data to client
		 * and disconnect.
		 */
		srv_accept(s);

		ssize_t rd = read(ctrl, &buff, sizeof(buff));
		UT_ASSERTeq(rd, sizeof(buff));

		srv_send(s, &buff, sizeof(buff));
		srv_disconnect(s);

	}

	close(ctrl);
	srv_stop(s);

}
