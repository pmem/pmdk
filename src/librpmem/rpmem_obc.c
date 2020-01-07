// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2019, Intel Corporation */

/*
 * rpmem_obc.c -- rpmem out-of-band connection client source file
 */

#include <stdlib.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "librpmem.h"
#include "rpmem.h"
#include "rpmem_common.h"
#include "rpmem_obc.h"
#include "rpmem_proto.h"
#include "rpmem_util.h"
#include "rpmem_ssh.h"
#include "out.h"
#include "sys_util.h"
#include "util.h"

/*
 * rpmem_obc -- rpmem out-of-band client connection handle
 */
struct rpmem_obc {
	struct rpmem_ssh *ssh;
};

/*
 * rpmem_obc_is_connected -- (internal) return non-zero value if client is
 * connected
 */
static inline int
rpmem_obc_is_connected(struct rpmem_obc *rpc)
{
	return rpc->ssh != NULL;
}

/*
 * rpmem_obc_check_ibc_attr -- (internal) check in-band connection
 * attributes
 */
static int
rpmem_obc_check_ibc_attr(struct rpmem_msg_ibc_attr *ibc)
{
	if (ibc->port == 0 || ibc->port > UINT16_MAX) {
		ERR("invalid port number received -- %u", ibc->port);
		errno = EPROTO;
		return -1;
	}

	if (ibc->persist_method != RPMEM_PM_GPSPM &&
		ibc->persist_method != RPMEM_PM_APM) {
		ERR("invalid persistency method received -- %u",
				ibc->persist_method);
		errno = EPROTO;
		return -1;
	}

	return 0;
}

/*
 * rpmem_obc_check_port -- (internal) verify target node port number
 */
static int
rpmem_obc_check_port(const struct rpmem_target_info *info)
{
	if (!(info->flags & RPMEM_HAS_SERVICE))
		return 0;

	if (*info->service == '\0') {
		ERR("invalid port number -- '%s'", info->service);
		goto err;
	}

	errno = 0;
	char *endptr;
	long port = strtol(info->service, &endptr, 10);
	if (errno || *endptr != '\0') {
		ERR("invalid port number -- '%s'", info->service);
		goto err;
	}

	if (port < 1) {
		ERR("port number must be positive -- '%s'", info->service);
		goto err;
	}

	if (port > UINT16_MAX) {
		ERR("port number too large -- '%s'", info->service);
		goto err;
	}

	return 0;
err:
	errno = EINVAL;
	return -1;
}

/*
 * rpmem_obc_close_conn -- (internal) close connection
 */
static void
rpmem_obc_close_conn(struct rpmem_obc *rpc)
{
	rpmem_ssh_close(rpc->ssh);

	(void) util_fetch_and_and64(&rpc->ssh, 0);
}

/*
 * rpmem_obc_init_msg_hdr -- (internal) initialize message header
 */
static void
rpmem_obc_set_msg_hdr(struct rpmem_msg_hdr *hdrp,
	enum rpmem_msg_type type, size_t size)
{
	hdrp->type = type;
	hdrp->size = size;
}

/*
 * rpmem_obc_set_pool_desc -- (internal) fill the pool descriptor field
 */
static void
rpmem_obc_set_pool_desc(struct rpmem_msg_pool_desc *pool_desc,
	const char *desc, size_t size)
{
	RPMEM_ASSERT(size <= UINT32_MAX);
	RPMEM_ASSERT(size > 0);

	pool_desc->size = (uint32_t)size;
	memcpy(pool_desc->desc, desc, size);
	pool_desc->desc[size - 1] = '\0';
}

/*
 * rpmem_obc_alloc_create_msg -- (internal) allocate and fill create request
 * message
 */
static struct rpmem_msg_create *
rpmem_obc_alloc_create_msg(const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr, size_t *msg_sizep)
{
	size_t pool_desc_size = strlen(req->pool_desc) + 1;
	size_t msg_size = sizeof(struct rpmem_msg_create) + pool_desc_size;
	struct rpmem_msg_create *msg = malloc(msg_size);
	if (!msg) {
		ERR("!cannot allocate create request message");
		return NULL;
	}

	rpmem_obc_set_msg_hdr(&msg->hdr, RPMEM_MSG_TYPE_CREATE, msg_size);

	msg->c.major = RPMEM_PROTO_MAJOR;
	msg->c.minor = RPMEM_PROTO_MINOR;
	msg->c.pool_size = req->pool_size;
	msg->c.nlanes = req->nlanes;
	msg->c.provider = req->provider;
	msg->c.buff_size = req->buff_size;

	rpmem_obc_set_pool_desc(&msg->pool_desc,
			req->pool_desc, pool_desc_size);

	if (pool_attr) {
		pack_rpmem_pool_attr(pool_attr, &msg->pool_attr);
	} else {
		RPMEM_LOG(INFO, "using zeroed pool attributes");
		memset(&msg->pool_attr, 0, sizeof(msg->pool_attr));
	}

	*msg_sizep = msg_size;
	return msg;
}

/*
 * rpmem_obc_check_req -- (internal) check request attributes
 */
static int
rpmem_obc_check_req(const struct rpmem_req_attr *req)
{
	if (req->provider >= MAX_RPMEM_PROV) {
		ERR("invalid provider specified -- %u", req->provider);
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/*
 * rpmem_obj_check_hdr_resp -- (internal) check response message header
 */
static int
rpmem_obc_check_hdr_resp(struct rpmem_msg_hdr_resp *resp,
	enum rpmem_msg_type type, size_t size)
{
	if (resp->type != type) {
		ERR("invalid message type received -- %u", resp->type);
		errno = EPROTO;
		return -1;
	}

	if (resp->size != size) {
		ERR("invalid message size received -- %lu", resp->size);
		errno = EPROTO;
		return -1;
	}

	if (resp->status >= MAX_RPMEM_ERR) {
		ERR("invalid status received -- %u", resp->status);
		errno = EPROTO;
		return -1;
	}

	if (resp->status) {
		enum rpmem_err status = (enum rpmem_err)resp->status;
		ERR("%s", rpmem_util_proto_errstr(status));
		errno = rpmem_util_proto_errno(status);
		return -1;
	}

	return 0;
}

/*
 * rpmem_obc_check_create_resp -- (internal) check create response message
 */
static int
rpmem_obc_check_create_resp(struct rpmem_msg_create_resp *resp)
{
	if (rpmem_obc_check_hdr_resp(&resp->hdr, RPMEM_MSG_TYPE_CREATE_RESP,
			sizeof(struct rpmem_msg_create_resp)))
		return -1;

	if (rpmem_obc_check_ibc_attr(&resp->ibc))
		return -1;

	return 0;
}

/*
 * rpmem_obc_get_res -- (internal) read response attributes
 */
static void
rpmem_obc_get_res(struct rpmem_resp_attr *res,
	struct rpmem_msg_ibc_attr *ibc)
{
	res->port = (unsigned short)ibc->port;
	res->rkey = ibc->rkey;
	res->raddr = ibc->raddr;
	res->persist_method =
		(enum rpmem_persist_method)ibc->persist_method;
	res->nlanes = ibc->nlanes;
}

/*
 * rpmem_obc_alloc_open_msg -- (internal) allocate and fill open request message
 */
static struct rpmem_msg_open *
rpmem_obc_alloc_open_msg(const struct rpmem_req_attr *req,
	const struct rpmem_pool_attr *pool_attr, size_t *msg_sizep)
{
	size_t pool_desc_size = strlen(req->pool_desc) + 1;
	size_t msg_size = sizeof(struct rpmem_msg_open) + pool_desc_size;
	struct rpmem_msg_open *msg = malloc(msg_size);
	if (!msg) {
		ERR("!cannot allocate open request message");
		return NULL;
	}

	rpmem_obc_set_msg_hdr(&msg->hdr, RPMEM_MSG_TYPE_OPEN, msg_size);

	msg->c.major = RPMEM_PROTO_MAJOR;
	msg->c.minor = RPMEM_PROTO_MINOR;
	msg->c.pool_size = req->pool_size;
	msg->c.nlanes = req->nlanes;
	msg->c.provider = req->provider;
	msg->c.buff_size = req->buff_size;

	rpmem_obc_set_pool_desc(&msg->pool_desc,
			req->pool_desc, pool_desc_size);

	*msg_sizep = msg_size;
	return msg;
}

/*
 * rpmem_obc_check_open_resp -- (internal) check open response message
 */
static int
rpmem_obc_check_open_resp(struct rpmem_msg_open_resp *resp)
{
	if (rpmem_obc_check_hdr_resp(&resp->hdr, RPMEM_MSG_TYPE_OPEN_RESP,
			sizeof(struct rpmem_msg_open_resp)))
		return -1;

	if (rpmem_obc_check_ibc_attr(&resp->ibc))
		return -1;

	return 0;
}

/*
 * rpmem_obc_check_close_resp -- (internal) check close response message
 */
static int
rpmem_obc_check_close_resp(struct rpmem_msg_close_resp *resp)
{
	if (rpmem_obc_check_hdr_resp(&resp->hdr, RPMEM_MSG_TYPE_CLOSE_RESP,
			sizeof(struct rpmem_msg_close_resp)))
		return -1;

	return 0;
}

/*
 * rpmem_obc_check_set_attr_resp -- (internal) check set attributes response
 * message
 */
static int
rpmem_obc_check_set_attr_resp(struct rpmem_msg_set_attr_resp *resp)
{
	if (rpmem_obc_check_hdr_resp(&resp->hdr, RPMEM_MSG_TYPE_SET_ATTR_RESP,
			sizeof(struct rpmem_msg_set_attr_resp)))
		return -1;

	return 0;
}

/*
 * rpmem_obc_init -- initialize rpmem obc handle
 */
struct rpmem_obc *
rpmem_obc_init(void)
{
	struct rpmem_obc *rpc = calloc(1, sizeof(*rpc));
	if (!rpc) {
		RPMEM_LOG(ERR, "!allocation of rpmem obc failed");
		return NULL;
	}

	return rpc;
}

/*
 * rpmem_obc_fini -- destroy rpmem obc handle
 *
 * This function must be called with connection already closed - after calling
 * the rpmem_obc_disconnect or after receiving relevant value from
 * rpmem_obc_monitor.
 */
void
rpmem_obc_fini(struct rpmem_obc *rpc)
{
	free(rpc);
}

/*
 * rpmem_obc_connect -- connect to target node
 *
 * Connects to target node, the target must be in the following format:
 * <addr>[:<port>]. If the port number is not specified the default
 * ssh port will be used. The <addr> is translated into IP address.
 *
 * Returns an error if connection is already established.
 */
int
rpmem_obc_connect(struct rpmem_obc *rpc, const struct rpmem_target_info *info)
{
	if (rpmem_obc_is_connected(rpc)) {
		errno = EALREADY;
		goto err_notconnected;
	}

	if (rpmem_obc_check_port(info))
		goto err_port;

	rpc->ssh = rpmem_ssh_open(info);
	if (!rpc->ssh)
		goto err_ssh_open;

	return 0;
err_ssh_open:
err_port:
err_notconnected:
	return -1;
}

/*
 * rpmem_obc_disconnect -- close the connection to target node
 *
 * Returns error if socket is not connected.
 */
int
rpmem_obc_disconnect(struct rpmem_obc *rpc)
{
	if (rpmem_obc_is_connected(rpc)) {
		rpmem_obc_close_conn(rpc);
		return 0;
	}

	errno = ENOTCONN;
	return -1;
}

/*
 * rpmem_obc_monitor -- monitor connection with target node
 *
 * The nonblock variable indicates whether this function should return
 * immediately (= 1) or may block (= 0).
 *
 * If the function detects that socket was closed by remote peer it is
 * closed on local side and set to -1, so there is no need to call
 * rpmem_obc_disconnect function. Please take a look at functions'
 * descriptions to see which functions cannot be used if the connection
 * has been already closed.
 *
 * This function expects there is no data pending on socket, if any data
 * is pending this function returns an error and sets errno to EPROTO.
 *
 * Return values:
 * 0   - not connected
 * 1   - connected
 * < 0 - error
 */
int
rpmem_obc_monitor(struct rpmem_obc *rpc, int nonblock)
{
	if (!rpmem_obc_is_connected(rpc))
		return 0;

	return rpmem_ssh_monitor(rpc->ssh, nonblock);
}

/*
 * rpmem_obc_create -- perform create request operation
 *
 * Returns error if connection has not been established yet.
 */
int
rpmem_obc_create(struct rpmem_obc *rpc,
	const struct rpmem_req_attr *req,
	struct rpmem_resp_attr *res,
	const struct rpmem_pool_attr *pool_attr)
{
	if (!rpmem_obc_is_connected(rpc)) {
		ERR("out-of-band connection not established");
		errno = ENOTCONN;
		goto err_notconnected;
	}

	if (rpmem_obc_check_req(req))
		goto err_req;

	size_t msg_size;
	struct rpmem_msg_create *msg =
		rpmem_obc_alloc_create_msg(req, pool_attr, &msg_size);
	if (!msg)
		goto err_alloc_msg;

	RPMEM_LOG(INFO, "sending create request message");

	rpmem_hton_msg_create(msg);
	if (rpmem_ssh_send(rpc->ssh, msg, msg_size)) {
		ERR("!sending create request message failed");
		goto err_msg_send;
	}

	RPMEM_LOG(NOTICE, "create request message sent");
	RPMEM_LOG(INFO, "receiving create request response");

	struct rpmem_msg_create_resp resp;
	if (rpmem_ssh_recv(rpc->ssh, &resp,
			sizeof(resp))) {
		ERR("!receiving create request response failed");
		goto err_msg_recv;
	}

	RPMEM_LOG(NOTICE, "create request response received");

	rpmem_ntoh_msg_create_resp(&resp);

	if (rpmem_obc_check_create_resp(&resp))
		goto err_msg_resp;

	rpmem_obc_get_res(res, &resp.ibc);

	free(msg);
	return 0;
err_msg_resp:
err_msg_recv:
err_msg_send:
	free(msg);
err_alloc_msg:
err_req:
err_notconnected:
	return -1;
}

/*
 * rpmem_obc_open -- perform open request operation
 *
 * Returns error if connection is not already established.
 */
int
rpmem_obc_open(struct rpmem_obc *rpc,
	const struct rpmem_req_attr *req,
	struct rpmem_resp_attr *res,
	struct rpmem_pool_attr *pool_attr)
{
	if (!rpmem_obc_is_connected(rpc)) {
		ERR("out-of-band connection not established");
		errno = ENOTCONN;
		goto err_notconnected;
	}

	if (rpmem_obc_check_req(req))
		goto err_req;

	size_t msg_size;
	struct rpmem_msg_open *msg =
		rpmem_obc_alloc_open_msg(req, pool_attr, &msg_size);
	if (!msg)
		goto err_alloc_msg;

	RPMEM_LOG(INFO, "sending open request message");

	rpmem_hton_msg_open(msg);
	if (rpmem_ssh_send(rpc->ssh, msg, msg_size)) {
		ERR("!sending open request message failed");
		goto err_msg_send;
	}

	RPMEM_LOG(NOTICE, "open request message sent");
	RPMEM_LOG(INFO, "receiving open request response");

	struct rpmem_msg_open_resp resp;
	if (rpmem_ssh_recv(rpc->ssh, &resp, sizeof(resp))) {
		ERR("!receiving open request response failed");
		goto err_msg_recv;
	}

	RPMEM_LOG(NOTICE, "open request response received");

	rpmem_ntoh_msg_open_resp(&resp);

	if (rpmem_obc_check_open_resp(&resp))
		goto err_msg_resp;

	rpmem_obc_get_res(res, &resp.ibc);
	if (pool_attr)
		unpack_rpmem_pool_attr(&resp.pool_attr, pool_attr);

	free(msg);
	return 0;
err_msg_resp:
err_msg_recv:
err_msg_send:
	free(msg);
err_alloc_msg:
err_req:
err_notconnected:
	return -1;
}

/*
 * rpmem_obc_set_attr -- perform set attributes request operation
 *
 * Returns error if connection is not already established.
 */
int
rpmem_obc_set_attr(struct rpmem_obc *rpc,
	const struct rpmem_pool_attr *pool_attr)
{
	if (!rpmem_obc_is_connected(rpc)) {
		ERR("out-of-band connection not established");
		errno = ENOTCONN;
		goto err_notconnected;
	}

	struct rpmem_msg_set_attr msg;
	rpmem_obc_set_msg_hdr(&msg.hdr, RPMEM_MSG_TYPE_SET_ATTR, sizeof(msg));
	if (pool_attr) {
		memcpy(&msg.pool_attr, pool_attr, sizeof(msg.pool_attr));
	} else {
		RPMEM_LOG(INFO, "using zeroed pool attributes");
		memset(&msg.pool_attr, 0, sizeof(msg.pool_attr));
	}

	RPMEM_LOG(INFO, "sending set attributes request message");

	rpmem_hton_msg_set_attr(&msg);
	if (rpmem_ssh_send(rpc->ssh, &msg, sizeof(msg))) {
		ERR("!sending set attributes request message failed");
		goto err_msg_send;
	}

	RPMEM_LOG(NOTICE, "set attributes request message sent");
	RPMEM_LOG(INFO, "receiving set attributes request response");

	struct rpmem_msg_set_attr_resp resp;
	if (rpmem_ssh_recv(rpc->ssh, &resp,
			sizeof(resp))) {
		ERR("!receiving set attributes request response failed");
		goto err_msg_recv;
	}

	RPMEM_LOG(NOTICE, "set attributes request response received");

	rpmem_ntoh_msg_set_attr_resp(&resp);

	if (rpmem_obc_check_set_attr_resp(&resp))
		goto err_msg_resp;

	return 0;
err_msg_resp:
err_msg_recv:
err_msg_send:
err_notconnected:
	return -1;
}

/*
 * rpmem_obc_close -- perform close request operation
 *
 * Returns error if connection is not already established.
 *
 * NOTE: this function does not close the connection, but sends close request
 * message to remote node and receives a response. The connection must be
 * closed using rpmem_obc_disconnect function.
 */
int
rpmem_obc_close(struct rpmem_obc *rpc, int flags)
{
	if (!rpmem_obc_is_connected(rpc)) {
		errno = ENOTCONN;
		return -1;
	}

	struct rpmem_msg_close msg;
	rpmem_obc_set_msg_hdr(&msg.hdr, RPMEM_MSG_TYPE_CLOSE, sizeof(msg));
	msg.flags = (uint32_t)flags;

	RPMEM_LOG(INFO, "sending close request message");

	rpmem_hton_msg_close(&msg);
	if (rpmem_ssh_send(rpc->ssh, &msg, sizeof(msg))) {
		RPMEM_LOG(ERR, "!sending close request failed");
		return -1;
	}

	RPMEM_LOG(NOTICE, "close request message sent");
	RPMEM_LOG(INFO, "receiving close request response");

	struct rpmem_msg_close_resp resp;
	if (rpmem_ssh_recv(rpc->ssh, &resp,
			sizeof(resp))) {
		RPMEM_LOG(ERR, "!receiving close request response failed");
		return -1;
	}

	RPMEM_LOG(NOTICE, "close request response received");

	rpmem_ntoh_msg_close_resp(&resp);

	if (rpmem_obc_check_close_resp(&resp))
		return -1;

	return 0;
}
