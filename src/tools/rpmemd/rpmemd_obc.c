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
 * rpmemd_obc.c -- rpmemd out-of-band connection definitions
 */

#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

#include "librpmem.h"
#include "rpmemd_log.h"
#include "rpmem_proto.h"
#include "rpmem_common.h"
#include "rpmemd_obc.h"

struct rpmemd_obc {
	int sockfd;
};

struct rpmemd_obc_client {
	int sockfd;
	struct sockaddr name;
	struct sockaddr peer;
	socklen_t addr_len;
};

/*
 * rpmemd_obc_check_proto_ver -- check protocol version
 */
static int
rpmemd_obc_check_proto_ver(unsigned major, unsigned minor)
{
	if (major != RPMEM_PROTO_MAJOR ||
	    minor != RPMEM_PROTO_MINOR) {
		RPMEMD_LOG(ERR, "unsupported protocol version -- %u.%u",
				major, minor);
		return -1;
	}

	return 0;
}

/*
 * rpmemd_obc_check_msg_hdr -- check message header
 */
static int
rpmemd_obc_check_msg_hdr(struct rpmem_msg_hdr *hdrp)
{
	switch (hdrp->type) {
	case RPMEM_MSG_TYPE_OPEN:
	case RPMEM_MSG_TYPE_CREATE:
	case RPMEM_MSG_TYPE_REMOVE:
	case RPMEM_MSG_TYPE_CLOSE:
		/* all messages from client to server are fine */
		break;
	default:
		RPMEMD_LOG(ERR, "invalid message type -- %u", hdrp->type);
		return -1;
	}

	if (hdrp->size < sizeof(struct rpmem_msg_hdr)) {
		RPMEMD_LOG(ERR, "invalid message size -- %lu", hdrp->size);
		return -1;
	}

	return 0;
}

/*
 * rpmemd_obc_check_pool_desc -- check pool descriptor
 */
static int
rpmemd_obc_check_pool_desc(struct rpmem_msg_hdr *hdrp, size_t msg_size,
	struct rpmem_msg_pool_desc *pool_desc)
{
	size_t body_size = msg_size + pool_desc->size;
	if (hdrp->size != body_size) {
		RPMEMD_LOG(ERR, "message and pool descriptor size mismatch "
			"-- is %lu should be %lu", hdrp->size, body_size);
		return -1;
	}

	if (pool_desc->size < 2) {
		RPMEMD_LOG(ERR, "invalid pool descriptor size -- %u "
				"(must be >= 2)", pool_desc->size);
		return -1;
	}

	if (pool_desc->desc[pool_desc->size - 1] != '\0') {
		RPMEMD_LOG(ERR, "invalid pool descriptor "
			"(must be NULL-terminated string)");
		return -1;
	}

	size_t len = strlen((char *)pool_desc->desc) + 1;

	if (pool_desc->size != len) {
		RPMEMD_LOG(ERR, "invalid pool descriptor size -- is %lu "
				"should be %u", len, pool_desc->size);
		return -1;
	}

	return 0;
}

/*
 * rpmemd_obc_check_provider -- check provider value
 */
static int
rpmemd_obc_check_provider(uint32_t provider)
{
	if (provider == 0 || provider >= MAX_RPMEM_PROV) {
		RPMEMD_LOG(ERR, "invalid provider -- %u", provider);
		return -1;
	}

	return 0;
}

/*
 * rpmemd_obc_ntoh_check_msg_create -- convert and check create request message
 */
static int
rpmemd_obc_ntoh_check_msg_create(struct rpmem_msg_hdr *hdrp)
{
	int ret;
	struct rpmem_msg_create *msg = (struct rpmem_msg_create *)hdrp;

	rpmem_ntoh_msg_create(msg);

	ret = rpmemd_obc_check_proto_ver(msg->major, msg->minor);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_pool_desc(hdrp, sizeof(*msg), &msg->pool_desc);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_provider(msg->provider);
	if (ret)
		return ret;

	return 0;
}

/*
 * rpmemd_obc_ntoh_check_msg_open -- convert and check open request message
 */
static int
rpmemd_obc_ntoh_check_msg_open(struct rpmem_msg_hdr *hdrp)
{
	int ret;
	struct rpmem_msg_open *msg = (struct rpmem_msg_open *)hdrp;

	rpmem_ntoh_msg_open(msg);

	ret = rpmemd_obc_check_proto_ver(msg->major, msg->minor);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_pool_desc(hdrp, sizeof(*msg), &msg->pool_desc);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_provider(msg->provider);
	if (ret)
		return ret;

	return 0;
}

/*
 * rpmemd_obc_ntoh_check_msg_remove -- convert and check remove request message
 */
static int
rpmemd_obc_ntoh_check_msg_remove(struct rpmem_msg_hdr *hdrp)
{
	int ret;
	struct rpmem_msg_remove *msg = (struct rpmem_msg_remove *)hdrp;

	rpmem_ntoh_msg_remove(msg);

	ret = rpmemd_obc_check_proto_ver(msg->major, msg->minor);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_pool_desc(hdrp, sizeof(*msg), &msg->pool_desc);
	if (ret)
		return ret;

	return 0;
}

/*
 * rpmemd_obc_ntoh_check_msg_close -- convert and check close request message
 */
static int
rpmemd_obc_ntoh_check_msg_close(struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_close *msg = (struct rpmem_msg_close *)hdrp;

	rpmem_ntoh_msg_close(msg);

	/* nothing to do */
	return 0;
}

typedef int (*rpmemd_obc_ntoh_check_msg_fn)(struct rpmem_msg_hdr *hdrp);

static rpmemd_obc_ntoh_check_msg_fn rpmemd_obc_ntoh_check_msg[] = {
	[RPMEM_MSG_TYPE_CREATE]	= rpmemd_obc_ntoh_check_msg_create,
	[RPMEM_MSG_TYPE_OPEN]	= rpmemd_obc_ntoh_check_msg_open,
	[RPMEM_MSG_TYPE_REMOVE]	= rpmemd_obc_ntoh_check_msg_remove,
	[RPMEM_MSG_TYPE_CLOSE]	= rpmemd_obc_ntoh_check_msg_close,
};

/*
 * rpmemd_obc_process_create -- process create request
 */
static int
rpmemd_obc_process_create(struct rpmemd_obc_client *client,
	struct rpmemd_obc_client_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_create *msg = (struct rpmem_msg_create *)hdrp;
	struct rpmem_req_attr req = {
		.pool_size = msg->pool_size,
		.nlanes = (unsigned)msg->nlanes,
		.pool_desc = (char *)msg->pool_desc.desc,
		.provider = (enum rpmem_provider)msg->provider,
	};

	return req_cb->create(client, arg, &req, &msg->pool_attr);
}

/*
 * rpmemd_obc_process_open -- process open request
 */
static int
rpmemd_obc_process_open(struct rpmemd_obc_client *client,
	struct rpmemd_obc_client_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_open *msg = (struct rpmem_msg_open *)hdrp;
	struct rpmem_req_attr req = {
		.pool_size = msg->pool_size,
		.nlanes = (unsigned)msg->nlanes,
		.pool_desc = (const char *)msg->pool_desc.desc,
		.provider = (enum rpmem_provider)msg->provider,
	};

	return req_cb->open(client, arg, &req);
}

/*
 * rpmemd_obc_process_remove -- process remove request
 */
static int
rpmemd_obc_process_remove(struct rpmemd_obc_client *client,
	struct rpmemd_obc_client_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_remove *msg = (struct rpmem_msg_remove *)hdrp;

	return req_cb->remove(client, arg, (const char *)msg->pool_desc.desc);
}

/*
 * rpmemd_obc_process_close -- process close request
 */
static int
rpmemd_obc_process_close(struct rpmemd_obc_client *client,
	struct rpmemd_obc_client_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	return req_cb->close(client, arg);
}

typedef int (*rpmemd_obc_process_fn)(struct rpmemd_obc_client *client,
		struct rpmemd_obc_client_requests *req_cb, void *arg,
		struct rpmem_msg_hdr *hdrp);

static rpmemd_obc_process_fn rpmemd_obc_process[] = {
	[RPMEM_MSG_TYPE_CREATE]	= rpmemd_obc_process_create,
	[RPMEM_MSG_TYPE_OPEN]	= rpmemd_obc_process_open,
	[RPMEM_MSG_TYPE_REMOVE]	= rpmemd_obc_process_remove,
	[RPMEM_MSG_TYPE_CLOSE]	= rpmemd_obc_process_close,
};

/*
 * rpmemd_obc_msg_recv -- receive and check request message
 *
 * Return values:
 * 0   - success
 * < 0 - error
 * 1   - client disconnected
 */
static int
rpmemd_obc_msg_recv(struct rpmemd_obc_client *client,
	struct rpmem_msg_hdr **hdrpp)
{
	struct rpmem_msg_hdr hdr;
	struct rpmem_msg_hdr nhdr;
	struct rpmem_msg_hdr *hdrp;
	int ret;

	ret = rpmem_obc_recv(client->sockfd, &nhdr, sizeof(nhdr));
	if (ret == 1) {
		RPMEMD_LOG(NOTICE, "client disconnected");
		return 1;
	}

	if (ret < 0) {
		RPMEMD_LOG(ERR, "!receiving message header failed");
		return ret;
	}

	memcpy(&hdr, &nhdr, sizeof(hdr));
	rpmem_ntoh_msg_hdr(&hdr);

	ret = rpmemd_obc_check_msg_hdr(&hdr);
	if (ret) {
		RPMEMD_LOG(ERR, "parsing message header failed");
		return ret;
	}

	hdrp = malloc(hdr.size);
	if (!hdrp) {
		RPMEMD_LOG(ERR, "!allocating message buffer failed");
		return -1;
	}

	memcpy(hdrp, &nhdr, sizeof(*hdrp));

	size_t body_size = hdr.size - sizeof(hdr);
	ret = rpmem_obc_recv(client->sockfd, hdrp->body, body_size);
	if (ret) {
		RPMEMD_LOG(ERR, "!receiving message body failed");
		goto err_recv_body;
	}

	ret = rpmemd_obc_ntoh_check_msg[hdr.type](hdrp);
	if (ret) {
		RPMEMD_LOG(ERR, "parsing message body failed");
		goto err_body;
	}

	*hdrpp = hdrp;
	return 0;
err_body:
err_recv_body:
	free(hdrp);
	return -1;
}

/*
 * rpmemd_obc_setsockopt -- set options on server's socket
 */
static int
rpmemd_obc_setsockopt(struct rpmemd_obc *rpdc)
{
	RPMEMD_ASSERT(rpdc->sockfd != -1);

	int optval;
	int ret;

	optval = 1;
	ret = setsockopt(rpdc->sockfd, SOL_SOCKET, SO_REUSEADDR,
			&optval, sizeof(optval));
	if (ret) {
		RPMEMD_LOG(ERR, "!setsockopt(SO_REUSEADDR)");
		return -1;
	}

	return 0;
}

/*
 * rpmemd_obc_init -- initialize rpmemd out-of-band connection server
 */
struct rpmemd_obc *
rpmemd_obc_init(void)
{
	RPMEMD_DBG("allocating connection server");

	struct rpmemd_obc *rpdc = calloc(1, sizeof(*rpdc));
	if (!rpdc) {
		RPMEMD_LOG(ERR, "!allocating connection server");
		return NULL;
	}

	rpdc->sockfd = -1;

	return rpdc;
}

/*
 * rpmemd_obc_fini -- destroy rpmemd out-of-band connection server
 */
void
rpmemd_obc_fini(struct rpmemd_obc *rpdc)
{
	RPMEMD_DBG("cleaning connection server");

	free(rpdc);
}

/*
 * rpmemd_obc_listen -- start listening
 */
int
rpmemd_obc_listen(struct rpmemd_obc *rpdc, int backlog,
	const char *node, const char *service)
{
	int ret;

	if (rpdc->sockfd != -1)
		RPMEMD_FATAL("server already listening");

	if (!service || strcmp(service, "0") == 0) {
		service = RPMEM_SERVICE;
		RPMEMD_DBG("using default port number -- %s", service);
	}

	struct addrinfo *ai;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;

	ret = getaddrinfo(node, service, &hints, &ai);
	if (ret) {
		if (ret == EAI_SYSTEM) {
			RPMEMD_LOG(ERR, "!%s:%s", node, service);
		} else {
			RPMEMD_LOG(ERR, "%s:%s: %s", node, service,
					gai_strerror(ret));
		}

		RPMEMD_LOG(ERR, "invalid address -- '%s:%s'", node, service);
		goto err_addrinfo;
	}

	struct sockaddr_in *in = (struct sockaddr_in *)ai->ai_addr;

	rpdc->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (rpdc->sockfd < 0) {
		RPMEMD_LOG(ERR, "!socket");
		goto err_socket;
	}

	if (rpmemd_obc_setsockopt(rpdc))
		goto err_setsockopt;

	ret = bind(rpdc->sockfd, ai->ai_addr, ai->ai_addrlen);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot bind address %s:%u",
				inet_ntoa(in->sin_addr), ntohs(in->sin_port));
		goto err_bind;
	}

	ret = listen(rpdc->sockfd, backlog);
	if (ret) {
		RPMEMD_LOG(ERR, "!cannot listen on %s:%u",
				inet_ntoa(in->sin_addr), ntohs(in->sin_port));
		goto err_listen;

	}

	freeaddrinfo(ai);

	return 0;
err_listen:
err_bind:
err_setsockopt:
	close(rpdc->sockfd);
	rpdc->sockfd = -1;
err_socket:
	freeaddrinfo(ai);
err_addrinfo:
	return -1;
}

/*
 * rpmemd_obc_close -- stop rpmemd out-of-band connection server
 */
int
rpmemd_obc_close(struct rpmemd_obc *rpdc)
{
	if (rpdc->sockfd == -1)
		RPMEMD_FATAL("server not listening");

	int ret = close(rpdc->sockfd);
	rpdc->sockfd = -1;
	return ret;
}

/*
 * rpmemd_obc_accept -- accept connection from client and return client handle
 */
struct rpmemd_obc_client *
rpmemd_obc_accept(struct rpmemd_obc *rpdc)
{
	if (rpdc->sockfd == -1)
		RPMEMD_FATAL("server not listening");

	struct rpmemd_obc_client *client = calloc(1, sizeof(*client));
	if (!client) {
		RPMEMD_LOG(ERR, "!allocating client failed");
		goto err_calloc;
	}

	socklen_t addrlen = sizeof(client->peer);
	client->sockfd = accept(rpdc->sockfd,
			&client->peer, &addrlen);
	if (client->sockfd < 0) {
		RPMEMD_LOG(ERR, "!accepting client failed");
		goto err_accept;
	}

	client->addr_len = addrlen;

	if (getsockname(client->sockfd, &client->name, &addrlen)) {
		RPMEMD_LOG(ERR, "!getting socket name");
		goto err_getsockname;
	}

	if (client->addr_len != addrlen) {
		RPMEMD_LOG(ERR, "getting socket name: address "
				"length mismatch");
		goto err_getsockname;
	}

	if (rpmem_obc_keepalive(client->sockfd)) {
		RPMEMD_LOG(ERR, "!enabling TCP keepalive failed");
		goto err_keepalive;
	}

	return client;
err_getsockname:
err_keepalive:
	close(client->sockfd);
err_accept:
	free(client);
err_calloc:
	return NULL;
}

/*
 * rpmemd_obc_client_is_connected -- return true if socket is connected
 */
int
rpmemd_obc_client_is_connected(struct rpmemd_obc_client *client)
{
	return client->sockfd != -1;
}

/*
 * rpmemd_obc_client_getpeer -- get remote client address
 */
int
rpmemd_obc_client_getpeer(struct rpmemd_obc_client *client,
	struct sockaddr *addr, socklen_t *addrlen)
{
	if (!rpmemd_obc_client_is_connected(client)) {
		errno = ENOTCONN;
		return -1;
	}

	if (*addrlen < client->addr_len) {
		errno = EINVAL;
		return -1;
	}

	*addrlen = client->addr_len;
	memcpy(addr, &client->peer, client->addr_len);

	return 0;
}

/*
 * rpmemd_obc_client_getname -- get local address
 */
int
rpmemd_obc_client_getname(struct rpmemd_obc_client *client,
	struct sockaddr *addr, socklen_t *addrlen)
{
	if (!rpmemd_obc_client_is_connected(client)) {
		errno = ENOTCONN;
		return -1;
	}

	if (*addrlen < client->addr_len) {
		errno = EINVAL;
		return -1;
	}

	*addrlen = client->addr_len;
	memcpy(addr, &client->name, client->addr_len);

	return 0;
}

/*
 * rpmemd_obc_client_fini -- destroy client
 */
void
rpmemd_obc_client_fini(struct rpmemd_obc_client *client)
{
	free(client);
}

/*
 * rpmemd_obc_client_close -- close out-of-band connection with client
 */
int
rpmemd_obc_client_close(struct rpmemd_obc_client *client)
{
	if (!rpmemd_obc_client_is_connected(client))
		RPMEMD_FATAL("client not connected");

	int ret = close(client->sockfd);
	if (!ret)
		client->sockfd = -1;

	return ret;
}

/*
 * rpmemd_obc_client_process -- wait for and process a message from client
 *
 * Return values:
 * 0   - success
 * < 0 - error
 * 1   - client disconnected
 */
int
rpmemd_obc_client_process(struct rpmemd_obc_client *client,
	struct rpmemd_obc_client_requests *req_cb, void *arg)
{
	RPMEMD_ASSERT(req_cb != NULL);
	RPMEMD_ASSERT(req_cb->create != NULL);
	RPMEMD_ASSERT(req_cb->open != NULL);
	RPMEMD_ASSERT(req_cb->remove != NULL);
	RPMEMD_ASSERT(req_cb->close != NULL);

	if (!rpmemd_obc_client_is_connected(client))
		RPMEMD_FATAL("client not connected");

	struct rpmem_msg_hdr *hdrp = NULL;
	int ret;

	ret = rpmemd_obc_msg_recv(client, &hdrp);
	if (ret)
		return ret;

	RPMEMD_ASSERT(hdrp != NULL);

	ret = rpmemd_obc_process[hdrp->type](client, req_cb, arg, hdrp);

	free(hdrp);

	return ret;
}

/*
 * rpmemd_obc_client_create_resp -- send create request response message
 */
int
rpmemd_obc_client_create_resp(struct rpmemd_obc_client *client,
	int status, const struct rpmem_resp_attr *res)
{
	struct rpmem_msg_create_resp resp = {
		.hdr = {
			.type	= RPMEM_MSG_TYPE_CREATE_RESP,
			.size	= sizeof(struct rpmem_msg_create_resp),
			.status	= (uint32_t)status,
		},
		.ibc = {
			.port	= res->port,
			.rkey	= res->rkey,
			.raddr	= res->raddr,
			.persist_method = res->persist_method,
			.nlanes = res->nlanes,
		},
	};

	rpmem_hton_msg_create_resp(&resp);

	return rpmem_obc_send(client->sockfd, &resp, sizeof(resp));
}

/*
 * rpmemd_obc_client_open_resp -- send open request response message
 */
int
rpmemd_obc_client_open_resp(struct rpmemd_obc_client *client,
	int status, const struct rpmem_resp_attr *res,
	const struct rpmem_pool_attr *pool_attr)
{
	struct rpmem_msg_open_resp resp = {
		.hdr = {
			.type	= RPMEM_MSG_TYPE_OPEN_RESP,
			.size	= sizeof(struct rpmem_msg_open_resp),
			.status	= (uint32_t)status,
		},
		.ibc = {
			.port	= res->port,
			.rkey	= res->rkey,
			.raddr	= res->raddr,
			.persist_method = res->persist_method,
			.nlanes = res->nlanes,
		},
		.pool_attr = *pool_attr,
	};

	rpmem_hton_msg_open_resp(&resp);

	return rpmem_obc_send(client->sockfd, &resp, sizeof(resp));
}

/*
 * rpmemd_obc_client_close_resp -- send close request response message
 */
int
rpmemd_obc_client_close_resp(struct rpmemd_obc_client *client,
	int status)
{
	struct rpmem_msg_close_resp resp = {
		.hdr = {
			.type	= RPMEM_MSG_TYPE_CLOSE_RESP,
			.size	= sizeof(struct rpmem_msg_close_resp),
			.status	= (uint32_t)status,
		},
	};

	rpmem_hton_msg_close_resp(&resp);

	return rpmem_obc_send(client->sockfd, &resp, sizeof(resp));
}

/*
 * rpmemd_obc_client_remove_resp -- send remove request response message
 */
int
rpmemd_obc_client_remove_resp(struct rpmemd_obc_client *client,
	int status)
{
	struct rpmem_msg_remove_resp resp = {
		.hdr = {
			.type	= RPMEM_MSG_TYPE_REMOVE_RESP,
			.size	= sizeof(struct rpmem_msg_remove_resp),
			.status	= (uint32_t)status,
		},
	};

	rpmem_hton_msg_remove_resp(&resp);

	return rpmem_obc_send(client->sockfd, &resp, sizeof(resp));
}
