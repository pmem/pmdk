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
 * rpmem_obc_test_common.c -- common definitions for rpmem_obc tests
 */

#include <sys/socket.h>
#include <netinet/in.h>

#include "rpmem_obc_test_common.h"

/*
 * srv_listen -- allocate a server and start listening on specified port
 */
struct server *
srv_listen(unsigned short port)
{
	struct server *s = CALLOC(1, sizeof(*s));
	s->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (s->fd == -1)
		UT_FATAL("!socket");

	int ret;

	int x = 1;
	ret = setsockopt(s->fd, SOL_SOCKET, SO_REUSEADDR, &x, sizeof(x));
	if (ret)
		UT_FATAL("!setsockopt");

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);

	ret = bind(s->fd, (struct sockaddr *)&addr, sizeof(addr));
	if (ret)
		UT_FATAL("!bind %u", port);

	ret = listen(s->fd, 1);
	if (ret)
		UT_FATAL("!listen");

	return s;
}

/*
 * srv_disconnect -- disconnect from a client
 */
void
srv_disconnect(struct server *s)
{
	CLOSE(s->cfd);
}

/*
 * srv_stop -- close the server
 */
void
srv_stop(struct server *s)
{
	CLOSE(s->fd);
	FREE(s);
}

/*
 * srv_accept -- accept connection from a client
 */
void
srv_accept(struct server *s)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	s->cfd = accept(s->fd, (struct sockaddr *)&client_addr,
			&client_len);
	UT_ASSERTne(s->cfd, -1);
}

/*
 * srv_recv -- read a message from the client
 */
void
srv_recv(struct server *s, void *buff, size_t len)
{
	size_t rd = 0;
	uint8_t *cbuf = buff;
	while (rd < len) {
		ssize_t ret = read(s->cfd, &cbuf[rd], len - rd);
		UT_ASSERT(ret > 0);
		rd += (size_t)ret;
	}
}

/*
 * srv_send -- send a message to the client
 */
void
srv_send(struct server *s, const void *buff, size_t len)
{
	size_t wr = 0;
	const uint8_t *cbuf = buff;
	while (wr < len) {
		ssize_t ret = write(s->cfd, &cbuf[wr], len - wr);
		UT_ASSERT(ret > 0);
		wr += (size_t)ret;
	}
}

/*
 * srv_get_port -- parse a port number
 */
unsigned short
srv_get_port(const char *str_port)
{
	unsigned short port = RPMEM_PORT;

	if (strcmp(str_port, "0") != 0)
		port = atoi(str_port);

	return port;
}

/*
 * srv_wait_disconnect -- wait for a client to disconnect
 */
void
srv_wait_disconnect(struct server *s)
{
	int buff;
	ssize_t rret = read(s->cfd, &buff, sizeof(buff));
	UT_ASSERT(rret <= 0);

	srv_disconnect(s);
}

/*
 * client_connect_wait -- wait until client connects to the server
 */
void
client_connect_wait(struct rpmem_obc *rpc, char *target)
{
	while (rpmem_obc_connect(rpc, target))
		;
}

/*
 * server_econnreset -- disconnect from client during performing an
 * operation
 */
void
server_econnreset(struct server *s, const void *msg, size_t len)
{
	for (int i = 0; i < ECONNRESET_LOOP; i++) {
		/*
		 * Update the ECONNRESET_COUNT macro if a number
		 * of the following cases has been changed.
		 */
		{
			/* 1. disconnect immediately */
			srv_accept(s);
			srv_disconnect(s);
		}
		{
			/* 2. disconnect after sending a half of message */
			srv_accept(s);
			srv_send(s, msg, len);
			srv_disconnect(s);
		}
	}
}
