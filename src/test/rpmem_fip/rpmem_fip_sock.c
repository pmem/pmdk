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
 * rpmem_fip_sock.c -- simple socket client-server for exchanging
 * required RDMA related data
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

#include "rpmem_common.h"
#include "rpmem_proto.h"
#include "rpmem_fip_sock.h"
#include "unittest.h"

/*
 * client_do_connect -- connects to remote host
 */
static int
client_do_connect(const char *node, const char *service,
	struct sockaddr_in *addr)
{
	struct addrinfo *addrinfo;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
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

		if (!connect(sockfd, ai->ai_addr, ai->ai_addrlen)) {
			UT_ASSERTeq(ai->ai_addrlen, sizeof(*addr));
			memcpy(addr, ai->ai_addr, sizeof(*addr));
			break;
		}

		close(sockfd);
		sockfd = -1;
	}

	freeaddrinfo(addrinfo);

	return sockfd;
}

/*
 * client_exchange -- connect to remote host and exchange required information
 */
int
client_exchange(const char *node, const char *service,
	unsigned nlanes,
	enum rpmem_provider provider,
	struct rpmem_resp_attr *resp,
	struct sockaddr_in *addr)
{
	int fd;

	fd = client_do_connect(node, service, addr);
	UT_ASSERTne(fd, -1);

	WRITE(fd, &nlanes, sizeof(nlanes));
	WRITE(fd, &provider, sizeof(provider));
	READ(fd, resp, sizeof(*resp));

	return fd;
}

/*
 * client_close -- close connection
 */
void
client_close(int fd)
{
	int cmd = 1;

	WRITE(fd, &cmd, sizeof(cmd));
	READ(fd, &cmd, sizeof(cmd));
	UT_ASSERTeq(cmd, 0);

	close(fd);
}

/*
 * server_exchange_begin -- accept a connection and read required information
 */
int
server_exchange_begin(const char *node, const char *service,
	unsigned *lanes, enum rpmem_provider *provider,
	struct sockaddr_in *addr_in)
{
	int ret;

	struct addrinfo *ai;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	ret = getaddrinfo(node, service, &hints, &ai);
	UT_ASSERTeq(ret, 0);
	UT_ASSERTeq(ai->ai_addrlen, sizeof(*addr_in));

	int fd = socket(AF_INET, SOCK_STREAM, 0);
	UT_ASSERT(fd >= 0);

	int x = 1;
	ret = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
	UT_ASSERTeq(ret, 0);

	ret = bind(fd, ai->ai_addr, ai->ai_addrlen);
	UT_ASSERTeq(ret, 0);

	memcpy(addr_in, ai->ai_addr, sizeof(*addr_in));

	ret = listen(fd, 1);
	UT_ASSERTeq(ret, 0);

	int cfd = accept(fd, NULL, NULL);
	UT_ASSERTne(cfd, -1);

	close(fd);

	READ(cfd, lanes, sizeof(*lanes));
	READ(cfd, provider, sizeof(*provider));

	freeaddrinfo(ai);

	return cfd;
}

/*
 * server_exchange_end -- send response to client
 */
void
server_exchange_end(int fd, struct rpmem_resp_attr resp)
{
	WRITE(fd, &resp, sizeof(resp));
}

/*
 * server_close_begin -- wait for close command
 */
void
server_close_begin(int fd)
{
	int cmd = 0;

	READ(fd, &cmd, sizeof(cmd));
	UT_ASSERTeq(cmd, 1);
}

/*
 * server_close_end -- send close response and wait for disconnect
 */
void
server_close_end(int fd)
{
	int cmd = 0;

	WRITE(fd, &cmd, sizeof(cmd));
	READ(fd, &cmd, sizeof(cmd));

	close(fd);
}
