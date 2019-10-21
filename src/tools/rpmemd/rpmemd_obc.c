/*
 * Copyright 2016-2019, Intel Corporation
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
	int fd_in;
	int fd_out;
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
	case RPMEM_MSG_TYPE_CLOSE:
	case RPMEM_MSG_TYPE_SET_ATTR:
		/* all messages from obc to server are fine */
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
			"(must be null-terminated string)");
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

	ret = rpmemd_obc_check_proto_ver(msg->c.major, msg->c.minor);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_pool_desc(hdrp, sizeof(*msg), &msg->pool_desc);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_provider(msg->c.provider);
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

	ret = rpmemd_obc_check_proto_ver(msg->c.major, msg->c.minor);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_pool_desc(hdrp, sizeof(*msg), &msg->pool_desc);
	if (ret)
		return ret;

	ret = rpmemd_obc_check_provider(msg->c.provider);
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

/*
 * rpmemd_obc_ntoh_check_msg_set_attr -- convert and check set attributes
 * request message
 */
static int
rpmemd_obc_ntoh_check_msg_set_attr(struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_set_attr *msg = (struct rpmem_msg_set_attr *)hdrp;

	rpmem_ntoh_msg_set_attr(msg);

	/* nothing to do */
	return 0;
}

typedef int (*rpmemd_obc_ntoh_check_msg_fn)(struct rpmem_msg_hdr *hdrp);

static rpmemd_obc_ntoh_check_msg_fn rpmemd_obc_ntoh_check_msg[] = {
	[RPMEM_MSG_TYPE_CREATE]		= rpmemd_obc_ntoh_check_msg_create,
	[RPMEM_MSG_TYPE_OPEN]		= rpmemd_obc_ntoh_check_msg_open,
	[RPMEM_MSG_TYPE_CLOSE]		= rpmemd_obc_ntoh_check_msg_close,
	[RPMEM_MSG_TYPE_SET_ATTR]	= rpmemd_obc_ntoh_check_msg_set_attr,
};

/*
 * rpmemd_obc_process_create -- process create request
 */
static int
rpmemd_obc_process_create(struct rpmemd_obc *obc,
	struct rpmemd_obc_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_create *msg = (struct rpmem_msg_create *)hdrp;
	struct rpmem_req_attr req = {
		.pool_size = msg->c.pool_size,
		.nlanes = (unsigned)msg->c.nlanes,
		.pool_desc = (char *)msg->pool_desc.desc,
		.provider = (enum rpmem_provider)msg->c.provider,
		.buff_size = msg->c.buff_size,
	};

	struct rpmem_pool_attr *rattr = NULL;
	struct rpmem_pool_attr rpmem_attr;
	unpack_rpmem_pool_attr(&msg->pool_attr, &rpmem_attr);
	if (!util_is_zeroed(&rpmem_attr, sizeof(rpmem_attr)))
		rattr = &rpmem_attr;

	return req_cb->create(obc, arg, &req, rattr);
}

/*
 * rpmemd_obc_process_open -- process open request
 */
static int
rpmemd_obc_process_open(struct rpmemd_obc *obc,
	struct rpmemd_obc_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_open *msg = (struct rpmem_msg_open *)hdrp;
	struct rpmem_req_attr req = {
		.pool_size = msg->c.pool_size,
		.nlanes = (unsigned)msg->c.nlanes,
		.pool_desc = (const char *)msg->pool_desc.desc,
		.provider = (enum rpmem_provider)msg->c.provider,
		.buff_size = msg->c.buff_size,
	};

	return req_cb->open(obc, arg, &req);
}

/*
 * rpmemd_obc_process_close -- process close request
 */
static int
rpmemd_obc_process_close(struct rpmemd_obc *obc,
	struct rpmemd_obc_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_close *msg = (struct rpmem_msg_close *)hdrp;
	return req_cb->close(obc, arg, (int)msg->flags);
}

/*
 * rpmemd_obc_process_set_attr -- process set attributes request
 */
static int
rpmemd_obc_process_set_attr(struct rpmemd_obc *obc,
	struct rpmemd_obc_requests *req_cb, void *arg,
	struct rpmem_msg_hdr *hdrp)
{
	struct rpmem_msg_set_attr *msg = (struct rpmem_msg_set_attr *)hdrp;
	struct rpmem_pool_attr *rattr = NULL;
	struct rpmem_pool_attr rpmem_attr;
	unpack_rpmem_pool_attr(&msg->pool_attr, &rpmem_attr);
	if (!util_is_zeroed(&rpmem_attr, sizeof(rpmem_attr)))
		rattr = &rpmem_attr;

	return req_cb->set_attr(obc, arg, rattr);
}

typedef int (*rpmemd_obc_process_fn)(struct rpmemd_obc *obc,
		struct rpmemd_obc_requests *req_cb, void *arg,
		struct rpmem_msg_hdr *hdrp);

static rpmemd_obc_process_fn rpmemd_obc_process_cb[] = {
	[RPMEM_MSG_TYPE_CREATE]		= rpmemd_obc_process_create,
	[RPMEM_MSG_TYPE_OPEN]		= rpmemd_obc_process_open,
	[RPMEM_MSG_TYPE_CLOSE]		= rpmemd_obc_process_close,
	[RPMEM_MSG_TYPE_SET_ATTR]	= rpmemd_obc_process_set_attr,
};

/*
 * rpmemd_obc_recv -- wrapper for read and decode data function
 */
static inline int
rpmemd_obc_recv(struct rpmemd_obc *obc, void *buff, size_t len)
{
	return rpmem_xread(obc->fd_in, buff, len, 0);
}

/*
 * rpmemd_obc_send -- wrapper for encode and write data function
 */
static inline int
rpmemd_obc_send(struct rpmemd_obc *obc, const void *buff, size_t len)
{
	return rpmem_xwrite(obc->fd_out, buff, len, 0);
}

/*
 * rpmemd_obc_msg_recv -- receive and check request message
 *
 * Return values:
 * 0   - success
 * < 0 - error
 * 1   - obc disconnected
 */
static int
rpmemd_obc_msg_recv(struct rpmemd_obc *obc,
	struct rpmem_msg_hdr **hdrpp)
{
	struct rpmem_msg_hdr hdr;
	struct rpmem_msg_hdr nhdr;
	struct rpmem_msg_hdr *hdrp;
	int ret;

	ret = rpmemd_obc_recv(obc, &nhdr, sizeof(nhdr));
	if (ret == 1) {
		RPMEMD_LOG(NOTICE, "out-of-band connection disconnected");
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
	ret = rpmemd_obc_recv(obc, hdrp->body, body_size);
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
 * rpmemd_obc_init -- initialize rpmemd
 */
struct rpmemd_obc *
rpmemd_obc_init(int fd_in, int fd_out)
{
	struct rpmemd_obc *obc = calloc(1, sizeof(*obc));
	if (!obc) {
		RPMEMD_LOG(ERR, "!allocating obc failed");
		goto err_calloc;
	}

	obc->fd_in = fd_in;
	obc->fd_out = fd_out;

	return obc;
err_calloc:
	return NULL;
}

/*
 * rpmemd_obc_fini -- destroy obc
 */
void
rpmemd_obc_fini(struct rpmemd_obc *obc)
{
	free(obc);
}

/*
 * rpmemd_obc_status -- sends initial status to the client
 */
int
rpmemd_obc_status(struct rpmemd_obc *obc, uint32_t status)
{
	return rpmemd_obc_send(obc, &status, sizeof(status));
}

/*
 * rpmemd_obc_process -- wait for and process a message from client
 *
 * Return values:
 * 0   - success
 * < 0 - error
 * 1   - client disconnected
 */
int
rpmemd_obc_process(struct rpmemd_obc *obc,
	struct rpmemd_obc_requests *req_cb, void *arg)
{
	RPMEMD_ASSERT(req_cb != NULL);
	RPMEMD_ASSERT(req_cb->create != NULL);
	RPMEMD_ASSERT(req_cb->open != NULL);
	RPMEMD_ASSERT(req_cb->close != NULL);
	RPMEMD_ASSERT(req_cb->set_attr != NULL);

	struct rpmem_msg_hdr *hdrp = NULL;
	int ret;

	ret = rpmemd_obc_msg_recv(obc, &hdrp);
	if (ret)
		return ret;

	RPMEMD_ASSERT(hdrp != NULL);

	ret = rpmemd_obc_process_cb[hdrp->type](obc, req_cb, arg, hdrp);

	free(hdrp);

	return ret;
}

/*
 * rpmemd_obc_create_resp -- send create request response message
 */
int
rpmemd_obc_create_resp(struct rpmemd_obc *obc,
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

	return rpmemd_obc_send(obc, &resp, sizeof(resp));
}

/*
 * rpmemd_obc_open_resp -- send open request response message
 */
int
rpmemd_obc_open_resp(struct rpmemd_obc *obc,
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
	};

	pack_rpmem_pool_attr(pool_attr, &resp.pool_attr);
	rpmem_hton_msg_open_resp(&resp);

	return rpmemd_obc_send(obc, &resp, sizeof(resp));
}

/*
 * rpmemd_obc_close_resp -- send close request response message
 */
int
rpmemd_obc_close_resp(struct rpmemd_obc *obc,
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

	return rpmemd_obc_send(obc, &resp, sizeof(resp));
}

/*
 * rpmemd_obc_set_attr_resp -- send set attributes request response message
 */
int
rpmemd_obc_set_attr_resp(struct rpmemd_obc *obc, int status)
{
	struct rpmem_msg_set_attr_resp resp = {
		.hdr = {
			.type	= RPMEM_MSG_TYPE_SET_ATTR_RESP,
			.size	= sizeof(struct rpmem_msg_set_attr_resp),
			.status	= (uint32_t)status,
		},
	};

	rpmem_hton_msg_set_attr_resp(&resp);

	return rpmemd_obc_send(obc, &resp, sizeof(resp));
}
