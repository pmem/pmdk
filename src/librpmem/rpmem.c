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
 * rpmem.c -- main source file for librpmem
 */
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <inttypes.h>

#include "librpmem.h"
#include "out.h"
#include "os.h"
#include "os_thread.h"
#include "util.h"
#include "rpmem.h"
#include "rpmem_common.h"
#include "rpmem_util.h"
#include "rpmem_obc.h"
#include "rpmem_fip.h"
#include "rpmem_fip_common.h"
#include "rpmem_ssh.h"
#include "rpmem_proto.h"

#define RPMEM_REMOVE_FLAGS_ALL (\
	RPMEM_REMOVE_FORCE |	\
	RPMEM_REMOVE_POOL_SET	\
)

#define RPMEM_CHECK_FORK() do {\
if (Rpmem_fork_unsafe) {\
	ERR("libfabric is initialized without fork() support");\
	return NULL;\
}\
} while (0)

static os_once_t Rpmem_fork_unsafe_key_once = OS_ONCE_INIT;

/*
 * rpmem_pool -- remote pool context
 */
struct rpmem_pool {
	struct rpmem_obc *obc;		/* out-of-band connection handle */
	struct rpmem_fip *fip;		/* fabric provider handle */
	struct rpmem_target_info *info;
	char fip_service[NI_MAXSERV];
	enum rpmem_provider provider;
	size_t max_wq_size;		/* max WQ size supported by provider */
	os_thread_t monitor;
	int closing;
	int no_headers;
	/*
	 * Last error code, need to be volatile because it can
	 * be accessed by multiple threads.
	 */
	volatile int error;
};

/*
 * env_get_bool -- parse value of specified environment variable as a bool
 *
 * Return values:
 *  0 - defined, valp has value
 *  1 - not defined
 * -1 - parsing error
 */
static int
env_get_bool(const char *name, int *valp)
{
	LOG(3, "name %s, valp %p", name, valp);

	const char *env = os_getenv(name);
	if (!env)
		return 1;

	char *endptr;
	errno = 0;
	long val = strtol(env, &endptr, 10);
	if (*endptr != '\0' || errno)
		goto err;

	if (val < INT_MIN || val > INT_MAX)
		goto err;

	*valp = (int)val;

	return 0;
err:
	RPMEM_LOG(ERR, "!parsing '%s' environment variable failed", name);
	return -1;
}

/*
 * rpmem_get_provider -- set provider based on node address and environment
 */
static int
rpmem_set_provider(RPMEMpool *rpp, const char *node)
{
	LOG(3, "rpp %p, node %s", rpp, node);

	struct rpmem_fip_probe probe;
	enum rpmem_provider prov = RPMEM_PROV_UNKNOWN;

	int ret = rpmem_fip_probe_get(node, &probe);
	if (ret)
		return -1;

	/*
	 * The sockets provider can be used only if specified environment
	 * variable is set to 1.
	 */
	if (rpmem_fip_probe(probe, RPMEM_PROV_LIBFABRIC_SOCKETS)) {
		int enable;
		ret = env_get_bool(RPMEM_PROV_SOCKET_ENV, &enable);
		if (!ret && enable) {
			prov = RPMEM_PROV_LIBFABRIC_SOCKETS;
		}
	}

	/*
	 * The verbs provider is enabled by default. If appropriate
	 * environment variable is set to 0, the verbs provider is disabled.
	 *
	 * The verbs provider has higher priority than sockets provider.
	 */
	if (rpmem_fip_probe(probe, RPMEM_PROV_LIBFABRIC_VERBS)) {
		int enable;
		ret = env_get_bool(RPMEM_PROV_VERBS_ENV, &enable);
		if (ret == 1 || (!ret && enable))
			prov = RPMEM_PROV_LIBFABRIC_VERBS;
	}

	if (prov == RPMEM_PROV_UNKNOWN)
		return -1;

	RPMEM_ASSERT(prov < MAX_RPMEM_PROV);
	rpp->max_wq_size = probe.max_wq_size[prov];
	rpp->provider = prov;

	return 0;
}

/*
 * rpmem_monitor_thread -- connection monitor background thread
 */
static void *
rpmem_monitor_thread(void *arg)
{
	LOG(3, "arg %p", arg);

	RPMEMpool *rpp = arg;

	int ret = rpmem_obc_monitor(rpp->obc, 0);
	if (ret && !rpp->closing) {
		RPMEM_LOG(ERR, "unexpected data received");
		rpp->error = errno;
	}

	return NULL;
}

/*
 * rpmem_common_init -- common routine for initialization
 */
static RPMEMpool *
rpmem_common_init(const char *target)
{
	LOG(3, "target %s", target);

	int ret;

	RPMEMpool *rpp = calloc(1, sizeof(*rpp));
	if (!rpp) {
		ERR("!calloc");
		goto err_malloc_rpmem;
	}

	rpp->info = rpmem_target_parse(target);
	if (!rpp->info) {
		ERR("!parsing target node address failed");
		goto err_target_split;
	}

	ret = rpmem_set_provider(rpp, rpp->info->node);
	if (ret) {
		errno = ENOMEDIUM;
		ERR("cannot find provider");
		goto err_provider;
	}

	RPMEM_LOG(NOTICE, "provider: %s", rpmem_provider_to_str(rpp->provider));

	if (rpp->provider == RPMEM_PROV_LIBFABRIC_SOCKETS) {
		/* libfabric's sockets provider does not support IPv6 */
		RPMEM_LOG(NOTICE, "forcing using IPv4");
		rpp->info->flags |= RPMEM_FLAGS_USE_IPV4;
	}

	rpp->obc = rpmem_obc_init();
	if (!rpp->obc) {
		ERR("!out-of-band connection initialization failed");
		goto err_obc_init;
	}

	RPMEM_LOG(INFO, "establishing out-of-band connection");

	ret = rpmem_obc_connect(rpp->obc, rpp->info);
	if (ret) {
		ERR("!out-of-band connection failed");
		goto err_obc_connect;
	}

	RPMEM_LOG(NOTICE, "out-of-band connection established");

	return rpp;
err_obc_connect:
	rpmem_obc_fini(rpp->obc);
err_obc_init:
err_provider:
	rpmem_target_free(rpp->info);
err_target_split:
	free(rpp);
err_malloc_rpmem:
	return NULL;
}

/*
 * rpmem_common_fini -- common routing for deinitialization
 */
static void
rpmem_common_fini(RPMEMpool *rpp, int join)
{
	LOG(3, "rpp %p, join %d", rpp, join);

	rpmem_obc_disconnect(rpp->obc);

	if (join) {
		int ret = os_thread_join(&rpp->monitor, NULL);
		if (ret) {
			errno = ret;
			ERR("joining monitor thread failed");
		}
	}

	rpmem_obc_fini(rpp->obc);

	rpmem_target_free(rpp->info);
	free(rpp);
}

/*
 * rpmem_common_fip_init -- common routine for initializing fabric provider
 */
static int
rpmem_common_fip_init(RPMEMpool *rpp, struct rpmem_req_attr *req,
	struct rpmem_resp_attr *resp, void *pool_addr, size_t pool_size,
	unsigned *nlanes, size_t buff_size)
{
	LOG(3, "rpp %p, req %p, resp %p, pool_addr %p, pool_size %zu, nlanes "
			"%p", rpp, req, resp, pool_addr, pool_size, nlanes);

	int ret;

	struct rpmem_fip_attr fip_attr = {
		.provider	= req->provider,
		.max_wq_size	= rpp->max_wq_size,
		.persist_method	= resp->persist_method,
		.laddr		= pool_addr,
		.size		= pool_size,
		.buff_size	= buff_size,
		.nlanes		= min(*nlanes, resp->nlanes),
		.raddr		= (void *)resp->raddr,
		.rkey		= resp->rkey,
	};

	ret = snprintf(rpp->fip_service, sizeof(rpp->fip_service),
			"%u", resp->port);
	if (ret <= 0) {
		ERR("snprintf: %d", ret);
		goto err_port;
	}

	rpp->fip = rpmem_fip_init(rpp->info->node, rpp->fip_service,
			&fip_attr, nlanes);
	if (!rpp->fip) {
		ERR("!in-band connection initialization failed");
		ret = -1;
		goto err_fip_init;
	}

	RPMEM_LOG(NOTICE, "final nlanes: %u", *nlanes);
	RPMEM_LOG(INFO, "establishing in-band connection");

	ret = rpmem_fip_connect(rpp->fip);
	if (ret) {
		ERR("!establishing in-band connection failed");
		goto err_fip_connect;
	}

	RPMEM_LOG(NOTICE, "in-band connection established");

	return 0;
err_fip_connect:
	rpmem_fip_fini(rpp->fip);
err_fip_init:
err_port:
	return ret;
}

/*
 * rpmem_common_fip_fini -- common routine for deinitializing fabric provider
 */
static void
rpmem_common_fip_fini(RPMEMpool *rpp)
{
	LOG(3, "rpp %p", rpp);

	RPMEM_LOG(INFO, "closing in-band connection");

	rpmem_fip_fini(rpp->fip);

	RPMEM_LOG(NOTICE, "in-band connection closed");
}

/*
 * rpmem_log_args -- log input arguments for rpmem_create and rpmem_open
 */
static void
rpmem_log_args(const char *req, const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned nlanes)
{
	LOG(3, "req %s, target %s, pool_set_name %s, pool_addr %p, pool_size "
			"%zu, nlanes %d", req, target, pool_set_name, pool_addr,
			pool_size, nlanes);

	RPMEM_LOG(NOTICE, "%s request:", req);
	RPMEM_LOG(NOTICE, "\ttarget: %s", target);
	RPMEM_LOG(NOTICE, "\tpool set: %s", pool_set_name);
	RPMEM_LOG(INFO, "\tpool addr: %p", pool_addr);
	RPMEM_LOG(INFO, "\tpool size: %lu", pool_size);
	RPMEM_LOG(NOTICE, "\tnlanes: %u", nlanes);
}

/*
 * rpmem_log_resp -- log response attributes
 */
static void
rpmem_log_resp(const char *req, const struct rpmem_resp_attr *resp)
{
	LOG(3, "req %s, resp %p", req, resp);

	RPMEM_LOG(NOTICE, "%s request response:", req);
	RPMEM_LOG(NOTICE, "\tnlanes: %u", resp->nlanes);
	RPMEM_LOG(NOTICE, "\tport: %u", resp->port);
	RPMEM_LOG(NOTICE, "\tpersist method: %s",
			rpmem_persist_method_to_str(resp->persist_method));
	RPMEM_LOG(NOTICE, "\tremote addr: 0x%" PRIx64, resp->raddr);
}

/*
 * rpmem_check_args -- validate user's arguments
 */
static int
rpmem_check_args(void *pool_addr, size_t pool_size, unsigned *nlanes)
{
	LOG(3, "pool_addr %p, pool_size %zu, nlanes %p", pool_addr, pool_size,
			nlanes);

	if (!pool_addr) {
		errno = EINVAL;
		ERR("invalid pool address");
		return -1;
	}

	if (!IS_PAGE_ALIGNED((uintptr_t)pool_addr)) {
		errno = EINVAL;
		ERR("Pool address must be aligned to page size (%llu)",
				Pagesize);
		return -1;
	}

	if (!IS_PAGE_ALIGNED(pool_size)) {
		errno = EINVAL;
		ERR("Pool size must be aligned to page size (%llu)",
				Pagesize);
		return -1;
	}

	if (!pool_size) {
		errno = EINVAL;
		ERR("invalid pool size");
		return -1;
	}

	if (!nlanes) {
		errno = EINVAL;
		ERR("lanes pointer cannot be NULL");
		return -1;
	}

	if (!(*nlanes)) {
		errno = EINVAL;
		ERR("number of lanes must be positive");
		return -1;
	}

	return 0;
}

/*
 * rpmem_create -- create remote pool on target node
 *
 * target        -- target node in format [<user>@]<target_name>[:<port>]
 * pool_set_name -- remote pool set name
 * pool_addr     -- local pool memory address which will be replicated
 * pool_size     -- required pool size
 * nlanes        -- number of lanes
 * create_attr   -- pool attributes used for creating the pool on remote node
 */
RPMEMpool *
rpmem_create(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	const struct rpmem_pool_attr *create_attr)
{
	LOG(3, "target %s, pool_set_name %s, pool_addr %p, pool_size %zu, "
			"nlanes %p, create_attr %p", target, pool_set_name,
			pool_addr, pool_size, nlanes, create_attr);

	os_once(&Rpmem_fork_unsafe_key_once, &rpmem_fip_probe_fork_safety);
	RPMEM_CHECK_FORK();

	rpmem_log_args("create", target, pool_set_name,
			pool_addr, pool_size, *nlanes);

	if (rpmem_check_args(pool_addr, pool_size, nlanes))
		return NULL;

	RPMEMpool *rpp = rpmem_common_init(target);
	if (!rpp)
		goto err_common_init;

	size_t buff_size = RPMEM_DEF_BUFF_SIZE;
	struct rpmem_req_attr req = {
		.pool_size	= pool_size,
		.nlanes		= min(*nlanes, Rpmem_max_nlanes),
		.provider	= rpp->provider,
		.pool_desc	= pool_set_name,
		.buff_size	= buff_size,
	};

	struct rpmem_resp_attr resp;
	int ret = rpmem_obc_create(rpp->obc, &req, &resp, create_attr);
	if (ret) {
		RPMEM_LOG(ERR, "!create request failed");
		goto err_obc_create;
	}

	if (create_attr == NULL ||
			util_is_zeroed(create_attr, sizeof(*create_attr)))
		rpp->no_headers = 1;

	rpmem_log_resp("create", &resp);

	ret = rpmem_common_fip_init(rpp, &req, &resp,
			pool_addr, pool_size, nlanes, buff_size);
	if (ret)
		goto err_fip_init;

	ret = os_thread_create(&rpp->monitor, NULL, rpmem_monitor_thread, rpp);
	if (ret) {
		errno = ret;
		ERR("!starting monitor thread");
		goto err_monitor;
	}

	return rpp;
err_monitor:
	rpmem_common_fip_fini(rpp);
err_fip_init:
	rpmem_obc_close(rpp->obc, RPMEM_CLOSE_FLAGS_REMOVE);
err_obc_create:
	rpmem_common_fini(rpp, 0);
err_common_init:
	return NULL;
}

/*
 * rpmem_open -- open remote pool on target node
 *
 * target        -- target node in format [<user>@]<target_name>[:<port>]
 * pool_set_name -- remote pool set name
 * pool_addr     -- local pool memory address which will be replicated
 * pool_size     -- required pool size
 * nlanes        -- number of lanes
 * open_attr     -- pool attributes, received from remote host
 */
RPMEMpool *
rpmem_open(const char *target, const char *pool_set_name,
	void *pool_addr, size_t pool_size, unsigned *nlanes,
	struct rpmem_pool_attr *open_attr)
{
	LOG(3, "target %s, pool_set_name %s, pool_addr %p, pool_size %zu, "
			"nlanes %p, create_attr %p", target, pool_set_name,
			pool_addr, pool_size, nlanes, open_attr);

	os_once(&Rpmem_fork_unsafe_key_once, &rpmem_fip_probe_fork_safety);
	RPMEM_CHECK_FORK();

	rpmem_log_args("open", target, pool_set_name,
			pool_addr, pool_size, *nlanes);

	if (rpmem_check_args(pool_addr, pool_size, nlanes))
		return NULL;

	RPMEMpool *rpp = rpmem_common_init(target);
	if (!rpp)
		goto err_common_init;

	size_t buff_size = RPMEM_DEF_BUFF_SIZE;
	struct rpmem_req_attr req = {
		.pool_size	= pool_size,
		.nlanes		= min(*nlanes, Rpmem_max_nlanes),
		.provider	= rpp->provider,
		.pool_desc	= pool_set_name,
		.buff_size	= buff_size,
	};

	struct rpmem_resp_attr resp;

	int ret = rpmem_obc_open(rpp->obc, &req, &resp, open_attr);
	if (ret) {
		RPMEM_LOG(ERR, "!open request failed");
		goto err_obc_create;
	}

	if (open_attr == NULL || util_is_zeroed(open_attr, sizeof(*open_attr)))
		rpp->no_headers = 1;

	rpmem_log_resp("open", &resp);

	ret = rpmem_common_fip_init(rpp, &req, &resp,
			pool_addr, pool_size, nlanes, buff_size);
	if (ret)
		goto err_fip_init;

	ret = os_thread_create(&rpp->monitor, NULL, rpmem_monitor_thread, rpp);
	if (ret) {
		errno = ret;
		ERR("!starting monitor thread");
		goto err_monitor;
	}

	return rpp;
err_monitor:
	rpmem_common_fip_fini(rpp);
err_fip_init:
	rpmem_obc_close(rpp->obc, 0);
err_obc_create:
	rpmem_common_fini(rpp, 0);
err_common_init:
	return NULL;
}

/*
 * rpmem_close -- close remote pool on target node
 */
int
rpmem_close(RPMEMpool *rpp)
{
	LOG(3, "rpp %p", rpp);

	RPMEM_LOG(INFO, "closing out-of-band connection");

	util_fetch_and_or32(&rpp->closing, 1);

	rpmem_fip_close(rpp->fip);

	int ret = rpmem_obc_close(rpp->obc, 0);
	if (ret)
		ERR("!close request failed");

	RPMEM_LOG(NOTICE, "out-of-band connection closed");

	rpmem_common_fip_fini(rpp);
	rpmem_common_fini(rpp, 1);

	return ret;
}

/*
 * rpmem_flush -- flush to target node operation
 *
 * rpp           -- remote pool handle
 * offset        -- offset in pool
 * length        -- length of flush operation
 * lane          -- lane number
 * flags         -- additional flags
 */
int
rpmem_flush(RPMEMpool *rpp, size_t offset, size_t length,
	unsigned lane, unsigned flags)
{
	LOG(3, "rpp %p, offset %zu, length %zu, lane %d, flags 0x%x",
			rpp, offset, length, lane, flags);

	if (unlikely(rpp->error)) {
		errno = rpp->error;
		return -1;
	}

	if (flags & RPMEM_FLUSH_FLAGS_MASK) {
		ERR("invalid flags (0x%x)", flags);
		errno = EINVAL;
		return -1;
	}

	if (rpp->no_headers == 0 && offset < RPMEM_HDR_SIZE) {
		ERR("offset (%zu) in pool is less than %d bytes", offset,
				RPMEM_HDR_SIZE);
		errno = EINVAL;
		return -1;
	}

	/*
	 * By default use RDMA SEND flush mode which has atomicity
	 * guarantees. For relaxed flush use RDMA WRITE.
	 */
	unsigned mode = RPMEM_PERSIST_SEND;
	if (flags & RPMEM_FLUSH_RELAXED)
		mode = RPMEM_FLUSH_WRITE;

	int ret = rpmem_fip_flush(rpp->fip, offset, length, lane, mode);
	if (unlikely(ret)) {
		LOG(2, "flush operation failed");
		rpp->error = ret;
		errno = rpp->error;
		return -1;
	}

	return 0;
}

/*
 * rpmem_drain -- drain on target node operation
 *
 * rpp           -- remote pool handle
 * lane          -- lane number
 * flags         -- additional flags
 */
int
rpmem_drain(RPMEMpool *rpp, unsigned lane, unsigned flags)
{
	LOG(3, "rpp %p, lane %d, flags 0x%x", rpp, lane, flags);

	if (unlikely(rpp->error)) {
		errno = rpp->error;
		return -1;
	}

	if (flags != 0) {
		ERR("invalid flags (0x%x)", flags);
		errno = EINVAL;
		return -1;
	}

	int ret = rpmem_fip_drain(rpp->fip, lane);
	if (unlikely(ret)) {
		LOG(2, "drain operation failed");
		rpp->error = ret;
		errno = rpp->error;
		return -1;
	}

	return 0;
}

/*
 * rpmem_persist -- persist operation on target node
 *
 * rpp           -- remote pool handle
 * offset        -- offset in pool
 * length        -- length of persist operation
 * lane          -- lane number
 */
int
rpmem_persist(RPMEMpool *rpp, size_t offset, size_t length,
	unsigned lane, unsigned flags)
{
	LOG(3, "rpp %p, offset %zu, length %zu, lane %d, flags 0x%x",
			rpp, offset, length, lane, flags);

	if (unlikely(rpp->error)) {
		errno = rpp->error;
		return -1;
	}

	if (flags & RPMEM_PERSIST_FLAGS_MASK) {
		ERR("invalid flags (0x%x)", flags);
		errno = EINVAL;
		return -1;
	}

	if (rpp->no_headers == 0 && offset < RPMEM_HDR_SIZE) {
		ERR("offset (%zu) in pool is less than %d bytes", offset,
				RPMEM_HDR_SIZE);
		errno = EINVAL;
		return -1;
	}

	/*
	 * By default use RDMA SEND persist mode which has atomicity
	 * guarantees. For relaxed persist use RDMA WRITE.
	 */
	unsigned mode = RPMEM_PERSIST_SEND;
	if (flags & RPMEM_PERSIST_RELAXED)
		mode = RPMEM_FLUSH_WRITE;

	int ret = rpmem_fip_persist(rpp->fip, offset, length,
			lane, mode);
	if (unlikely(ret)) {
		LOG(2, "persist operation failed");
		rpp->error = ret;
		errno = rpp->error;
		return -1;
	}

	return 0;
}

/*
 * rpmem_deep_persist -- deep flush operation on target node
 *
 * rpp           -- remote pool handle
 * offset        -- offset in pool
 * length        -- length of deep flush operation
 * lane          -- lane number
 */
int
rpmem_deep_persist(RPMEMpool *rpp, size_t offset, size_t length, unsigned lane)
{
	LOG(3, "rpp %p, offset %zu, length %zu, lane %d", rpp, offset, length,
			lane);

	if (unlikely(rpp->error)) {
		errno = rpp->error;
		return -1;
	}

	if (offset < RPMEM_HDR_SIZE) {
		ERR("offset (%zu) in pool is less than %d bytes", offset,
				RPMEM_HDR_SIZE);
		errno = EINVAL;
		return -1;
	}

	int ret = rpmem_fip_persist(rpp->fip, offset, length,
			lane, RPMEM_DEEP_PERSIST);
	if (unlikely(ret)) {
		ERR("persist operation failed");
		rpp->error = ret;
		errno = rpp->error;
		return -1;
	}

	return 0;
}

/*
 * rpmem_read -- read data from remote pool:
 *
 * rpp           -- remote pool handle
 * buff          -- output buffer
 * offset        -- offset in pool
 * length        -- length of read operation
 */
int
rpmem_read(RPMEMpool *rpp, void *buff, size_t offset,
	size_t length, unsigned lane)
{
	LOG(3, "rpp %p, buff %p, offset %zu, length %zu, lane %d", rpp, buff,
			offset, length, lane);

	if (unlikely(rpp->error)) {
		errno = rpp->error;
		return -1;
	}

	if (rpp->no_headers == 0 && offset < RPMEM_HDR_SIZE)
		LOG(1, "reading from pool at offset (%zu) less than %d bytes",
				offset, RPMEM_HDR_SIZE);

	int ret = rpmem_fip_read(rpp->fip, buff, length, offset, lane);
	if (unlikely(ret)) {
		errno = ret;
		ERR("!read operation failed");
		rpp->error = ret;
		return -1;
	}

	return 0;
}

/*
 * rpmem_set_attr -- overwrite pool attributes on the remote node
 *
 * rpp           -- remote pool handle
 * attr          -- new pool attributes for the pool on remote node
 */
int
rpmem_set_attr(RPMEMpool *rpp, const struct rpmem_pool_attr *attr)
{
	LOG(3, "rpp %p, attr %p", rpp, attr);

	if (unlikely(rpp->error)) {
		errno = rpp->error;
		return -1;
	}

	int ret = rpmem_obc_set_attr(rpp->obc, attr);
	if (ret) {
		RPMEM_LOG(ERR, "!set attributes request failed");
	}
	return ret;
}

/*
 * rpmem_remove -- remove pool from remote node
 *
 * target        -- target node in format [<user>@]<target_name>[:<port>]
 * pool_set_name -- remote pool set name
 * flags         -- bitwise OR of one or more of the following flags:
 *  - RPMEM_REMOVE_FORCE
 *  - RPMEM_REMOVE_POOL_SET
 */
int
rpmem_remove(const char *target, const char *pool_set, int flags)
{
	LOG(3, "target %s, pool_set %s, flags %d", target, pool_set, flags);

	if (flags & ~(RPMEM_REMOVE_FLAGS_ALL)) {
		ERR("invalid flags specified");
		errno = EINVAL;
		return -1;
	}

	struct rpmem_target_info *info = rpmem_target_parse(target);
	if (!info) {
		ERR("!parsing target node address failed");
		goto err_target;
	}

	const char *argv[5];
	argv[0] = "--remove";
	argv[1] = pool_set;
	const char **cur = &argv[2];

	if (flags & RPMEM_REMOVE_FORCE)
		*cur++ = "--force";

	if (flags & RPMEM_REMOVE_POOL_SET)
		*cur++ = "--pool-set";

	*cur = NULL;

	struct rpmem_ssh *ssh = rpmem_ssh_execv(info, argv);
	if (!ssh) {
		ERR("!executing ssh command failed");
		goto err_ssh_exec;
	}

	int ret;

	ret = rpmem_ssh_monitor(ssh, 0);
	if (ret) {
		ERR("!waiting for remote command failed");
		goto err_ssh_monitor;
	}

	ret = rpmem_ssh_close(ssh);
	if (ret) {
		errno = ret;
		ERR("remote command failed");
		goto err_ssh_close;
	}

	rpmem_target_free(info);

	return 0;
err_ssh_monitor:
	rpmem_ssh_close(ssh);
err_ssh_close:
err_ssh_exec:
	rpmem_target_free(info);
err_target:
	return -1;
}

#if FAULT_INJECTION
void
rpmem_inject_fault_at(enum pmem_allocation_type type, int nth,
						const char *at)
{
	return common_inject_fault_at(type, nth, at);
}

int
rpmem_fault_injection_enabled(void)
{
	return common_fault_injection_enabled();
}
#endif
