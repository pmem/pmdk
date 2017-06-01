/*
 * Copyright 2016-2017, Intel Corporation
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
 * rpmemd_fip.c -- rpmemd libfabric provider module source file
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>

#include "rpmemd_log.h"

#include "rpmem_common.h"
#include "rpmem_proto.h"
#include "rpmem_fip_msg.h"
#include "rpmem_fip_common.h"
#include "rpmemd_fip.h"

#include "os_thread.h"
#include "util.h"
#include "valgrind_internal.h"

#define RPMEMD_FI_ERR(e, fmt, args...)\
	RPMEMD_LOG(ERR, fmt ": %s", ## args, fi_strerror((e)))

#define RPMEMD_FI_CLOSE(f, fmt, args...) (\
{\
	int ret = fi_close(&(f)->fid);\
	if (ret)\
		RPMEMD_FI_ERR(ret, fmt, ## args);\
	ret;\
})

/*
 * rpmem_fip_lane -- base lane structure
 */
struct rpmem_fip_lane {
	struct fid_ep *ep;
	struct fid_cq *cq;
	uint64_t event;
};

/*
 * rpmem_fip_lane_begin -- initialize lane events
 */
static inline void
rpmem_fip_lane_begin(struct rpmem_fip_lane *lanep, uint64_t event)
{
	lanep->event = event;
}

/*
 * rpmemd_fip_lane -- daemon's lane for GPSPM
 */
struct rpmemd_fip_lane {
	struct rpmem_fip_lane base;	/* lane base structure */
	struct rpmem_fip_msg recv;	/* RECV message */
	struct rpmem_fip_msg send;	/* SEND message */
	struct rpmemd_fip_worker *worker; /* lane's worker */
};

typedef int (*rpmemd_fip_init_fn)(struct rpmemd_fip *fip);
typedef int (*rpmemd_fip_lane_fn)(struct rpmemd_fip *fip,
		struct rpmemd_fip_lane *lanep);
typedef int (*rpmemd_fip_fini_fn)(struct rpmemd_fip *fip);
typedef int (*rpmemd_fip_process_fn)(struct rpmemd_fip *fip);

/*
 * rpmemd_fip_ops -- operations specific for persistency method
 */
struct rpmemd_fip_ops {
	rpmemd_fip_init_fn init;
	rpmemd_fip_fini_fn fini;
	rpmemd_fip_lane_fn post;
	rpmemd_fip_process_fn process_start;
	rpmemd_fip_process_fn process_stop;
};

struct rpmemd_fip_worker {
	struct rpmemd_fip *fip;
	os_thread_t thread;
	struct rpmemd_fip_lane *lanep;
};

/*
 * rpmemd_fip -- main context of rpmemd_fip
 */
struct rpmemd_fip {
	struct fi_info *fi;		/* fabric interface information */
	struct fid_fabric *fabric;	/* fabric domain */
	struct fid_domain *domain;	/* fabric protection domain */
	struct fid_eq *eq;		/* event queue */
	struct fid_pep *pep;		/* passive endpoint - listener */
	struct fid_mr *mr;		/* memory region for pool */
	struct rpmemd_fip_ops *ops;	/* ops specific for persist method */

	int (*persist)(const void *addr, size_t len);	/* persist function */
	void *addr;			/* pool's address */
	size_t size;			/* size of the pool */
	enum rpmem_persist_method persist_method;

	volatile int closing;	/* flag for closing background threads */
	unsigned nlanes;	/* number of lanes */
	size_t nthreads;	/* number of threads for processing */
	size_t cq_size;		/* size of completion queue */

	/* the following fields are used only for GPSPM */
	struct rpmemd_fip_lane *lanes;
	struct rpmem_fip_lane rd_lane;

	struct rpmem_msg_persist *pmsg;	/* persist message buffer */
	struct fid_mr *pmsg_mr;		/* persist message memory region */
	void *pmsg_mr_desc;		/* persist message local descriptor */

	struct rpmem_msg_persist_resp *pres; /* persist response buffer */
	struct fid_mr *pres_mr;		/* persist response memory region */
	void *pres_mr_desc;		/* persist response local descriptor */

	struct rpmemd_fip_worker *workers;	/* process workers */
};

/*
 * rpmemd_fip_set_nlanes -- set required number of lanes based on fabric
 * interface information and persistency method
 */
static void
rpmemd_fip_set_nlanes(struct rpmemd_fip *fip, unsigned nlanes)
{
	size_t max_nlanes = rpmem_fip_max_nlanes(fip->fi);
	RPMEMD_ASSERT(max_nlanes < UINT_MAX);

	fip->nlanes = min((unsigned)max_nlanes, nlanes);
}

/*
 * rpmemd_fip_getinfo -- obtain fabric interface information
 */
static int
rpmemd_fip_getinfo(struct rpmemd_fip *fip, const char *service,
	const char *node, enum rpmem_provider provider)
{
	int ret;

	struct fi_info *hints = rpmem_fip_get_hints(provider);
	if (!hints) {
		RPMEMD_LOG(ERR, "getting fabric interface hints");
		ret = -1;
		goto err_fi_get_hints;
	}

	ret = fi_getinfo(RPMEM_FIVERSION, node, service, FI_SOURCE,
			hints, &fip->fi);
	if (ret) {
		RPMEMD_FI_ERR(ret, "getting fabric interface information");
		goto err_fi_getinfo;
	}

	rpmem_fip_print_info(fip->fi);

	fi_freeinfo(hints);
	return 0;
err_fi_getinfo:
	fi_freeinfo(hints);
err_fi_get_hints:
	return ret;
}

/*
 * rpmemd_fip_set_resp -- fill the response structure
 */
static int
rpmemd_fip_set_resp(struct rpmemd_fip *fip, struct rpmem_resp_attr *resp)
{
	int ret;
	if (fip->fi->addr_format == FI_SOCKADDR_IN) {
		struct sockaddr_in addr_in;
		size_t addrlen = sizeof(addr_in);

		ret = fi_getname(&fip->pep->fid, &addr_in, &addrlen);
		if (ret) {
			RPMEMD_FI_ERR(ret, "getting local endpoint address");
			goto err_fi_getname;
		}

		if (!addr_in.sin_port) {
			RPMEMD_LOG(ERR, "dynamic allocation of port failed");
			goto err_port;
		}

		resp->port = htons(addr_in.sin_port);
	} else if (fip->fi->addr_format == FI_SOCKADDR_IN6) {
		struct sockaddr_in6 addr_in6;
		size_t addrlen = sizeof(addr_in6);

		ret = fi_getname(&fip->pep->fid, &addr_in6, &addrlen);
		if (ret) {
			RPMEMD_FI_ERR(ret, "getting local endpoint address");
			goto err_fi_getname;
		}

		if (!addr_in6.sin6_port) {
			RPMEMD_LOG(ERR, "dynamic allocation of port failed");
			goto err_port;
		}

		resp->port = htons(addr_in6.sin6_port);
	} else {
		RPMEMD_LOG(ERR, "invalid address format");
		return -1;
	}

	resp->rkey = fi_mr_key(fip->mr);
	resp->persist_method = fip->persist_method;
	resp->raddr = (uint64_t)fip->addr;
	resp->nlanes = fip->nlanes;

	return 0;
err_port:
err_fi_getname:
	return -1;
}

/*
 * rpmemd_fip_init_fabric_res -- initialize common fabric's resources
 */
static int
rpmemd_fip_init_fabric_res(struct rpmemd_fip *fip)
{
	int ret;
	ret = fi_fabric(fip->fi->fabric_attr, &fip->fabric, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening fabric domain");
		goto err_fi_fabric;
	}

	ret = fi_domain(fip->fabric, fip->fi, &fip->domain, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening fabric access domain");
		goto err_fi_domain;
	}

	struct fi_eq_attr eq_attr = {
		.size = 0,	/* use default */
		.flags = 0,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_set = NULL,
	};

	ret = fi_eq_open(fip->fabric, &eq_attr, &fip->eq, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening event queue");
		goto err_eq_open;
	}

	ret = fi_passive_ep(fip->fabric, fip->fi, &fip->pep, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "allocating passive endpoint");
		goto err_pep;
	}

	ret = fi_pep_bind(fip->pep, &fip->eq->fid, 0);
	if (ret) {
		RPMEMD_FI_ERR(ret, "binding event queue to passive endpoint");
		goto err_pep_bind_eq;
	}

	return 0;
err_pep_bind_eq:
	RPMEMD_FI_CLOSE(fip->pep, "closing passive endpoint");
err_pep:
	RPMEMD_FI_CLOSE(fip->eq, "closing event queue");
err_eq_open:
	RPMEMD_FI_CLOSE(fip->domain, "closing fabric access domain");
err_fi_domain:
	RPMEMD_FI_CLOSE(fip->fabric, "closing fabric domain");
err_fi_fabric:
	return ret;
}

/*
 * rpmemd_fip_fini_fabric_res -- deinitialize common fabric resources
 */
static void
rpmemd_fip_fini_fabric_res(struct rpmemd_fip *fip)
{
	RPMEMD_FI_CLOSE(fip->pep, "closing passive endpoint");
	RPMEMD_FI_CLOSE(fip->eq, "closing event queue");
	RPMEMD_FI_CLOSE(fip->domain, "closing fabric access domain");
	RPMEMD_FI_CLOSE(fip->fabric, "closing fabric domain");
}

/*
 * rpmemd_fip_init_memory -- initialize memory pool's resources
 */
static int
rpmemd_fip_init_memory(struct rpmemd_fip *fip)
{
	int ret;

	/*
	 * Register memory region with appropriate access bits:
	 * - FI_REMOTE_READ  - remote peer can issue READ operation,
	 * - FI_REMOTE_WRITE - remote peer can issue WRITE operation,
	 */
	ret = fi_mr_reg(fip->domain, fip->addr, fip->size,
			FI_REMOTE_READ | FI_REMOTE_WRITE, 0, 0, 0,
			&fip->mr, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "registering memory");
		return -1;
	}

	return 0;
}

/*
 * rpmemd_fip_fini_memory -- deinitialize memory pool's resources
 */
static void
rpmemd_fip_fini_memory(struct rpmemd_fip *fip)
{
	RPMEMD_FI_CLOSE(fip->mr, "unregistering memory");
}

/*
 * rpmemd_fip_init_ep -- initialize active endpoint
 */
static int
rpmemd_fip_init_ep(struct rpmemd_fip *fip, struct fi_info *info,
	struct rpmem_fip_lane *lanep)
{
	int ret;

	info->tx_attr->size = rpmem_fip_tx_size(fip->persist_method,
			RPMEM_FIP_NODE_SERVER);

	info->rx_attr->size = rpmem_fip_rx_size(fip->persist_method,
			RPMEM_FIP_NODE_SERVER);

	/* create an endpoint from fabric interface info */
	ret = fi_endpoint(fip->domain, info, &lanep->ep, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "allocating endpoint");
		goto err_endpoint;
	}

	/* bind event queue to the endpoint */
	ret = fi_ep_bind(lanep->ep, &fip->eq->fid, 0);
	if (ret) {
		RPMEMD_FI_ERR(ret, "binding event queue to endpoint");
		goto err_bind_eq;
	}

	/*
	 * Bind completion queue to the endpoint.
	 * Use a single completion queue for outbound and inbound work
	 * requests. Use selective completion implies adding FI_COMPLETE
	 * flag to each WR which needs a completion.
	 */
	ret = fi_ep_bind(lanep->ep, &lanep->cq->fid,
			FI_RECV | FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
	if (ret) {
		RPMEMD_FI_ERR(ret, "binding completion queue to endpoint");
		goto err_bind_cq;
	}

	/* enable the endpoint */
	ret = fi_enable(lanep->ep);
	if (ret) {
		RPMEMD_FI_ERR(ret, "enabling endpoint");
		goto err_enable;
	}

	return 0;
err_enable:
err_bind_cq:
err_bind_eq:
	RPMEMD_FI_CLOSE(lanep->ep, "closing endpoint");
err_endpoint:
	return -1;
}

/*
 * rpmemd_fip_fini_ep -- close endpoint
 */
static int
rpmemd_fip_fini_ep(struct rpmem_fip_lane *lanep)
{
	return RPMEMD_FI_CLOSE(lanep->ep, "closing endpoint");
}

/*
 * rpmemd_fip_init_apm -- initialize APM resources
 */
static int
rpmemd_fip_init_apm(struct rpmemd_fip *fip)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmemd_fip_fini_apm -- deinitialize APM resources
 */
static int
rpmemd_fip_fini_apm(struct rpmemd_fip *fip)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmemd_fip_fini_apm -- post work requests for APM
 */
static int
rpmemd_fip_post_apm(struct rpmemd_fip *fip, struct rpmemd_fip_lane *lanep)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmemd_fip_process_start_apm -- start processing APM
 */
static int
rpmemd_fip_process_start_apm(struct rpmemd_fip *fip)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmemd_fip_process_stop_apm -- stop processing APM
 */
static int
rpmemd_fip_process_stop_apm(struct rpmemd_fip *fip)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmemd_fip_gpspm_post_msg -- post RECV buffer for GPSPM
 */
static inline int
rpmemd_fip_gpspm_post_msg(struct rpmemd_fip_lane *lanep)
{
	int ret = rpmem_fip_recvmsg(lanep->base.ep, &lanep->recv);
	if (ret) {
		RPMEMD_FI_ERR(ret, "posting GPSPM recv buffer");
		return ret;
	}

	return 0;
}

/*
 * rpmemd_fip_gpspm_post_resp -- post SEND buffer for GPSPM
 */
static inline int
rpmemd_fip_gpspm_post_resp(struct rpmemd_fip_lane *lanep)
{
	int ret = rpmem_fip_sendmsg(lanep->base.ep, &lanep->send);
	if (ret) {
		RPMEMD_FI_ERR(ret, "posting GPSPM send buffer");
		return ret;
	}

	return 0;
}

/*
 * rpmemd_fip_post_gpspm -- post all RECV messages
 */
static int
rpmemd_fip_post_gpspm(struct rpmemd_fip *fip, struct rpmemd_fip_lane *lanep)
{
	int ret = rpmem_fip_recvmsg(lanep->base.ep, &lanep->recv);
	if (ret) {
		RPMEMD_FI_ERR(ret, "posting GPSPM recv buffer");
		return ret;
	}

	return 0;
}

/*
 * rpmemd_fip_lane_init -- initialize single lane
 */
static int
rpmemd_fip_lane_init(struct rpmemd_fip *fip, struct rpmem_fip_lane *lanep)
{
	struct fi_cq_attr cq_attr = {
		.size = fip->cq_size,
		.flags = 0,
		.format = FI_CQ_FORMAT_MSG, /* need context and flags */
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_cond = FI_CQ_COND_NONE,
		.wait_set = NULL,
	};

	int ret = fi_cq_open(fip->domain, &cq_attr, &lanep->cq, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "opening completion queue");
		goto err_cq_open;
	}

	return 0;
err_cq_open:
	return -1;
}

/*
 * rpmemd_fip_lane_fini -- deinitialize single lane
 */
static void
rpmemd_fip_lane_fini(struct rpmemd_fip *fip, struct rpmem_fip_lane *lanep)
{
	RPMEMD_FI_CLOSE(lanep->cq, "closing completion queue");
}

/*
 * rpmemd_fip_lanes_init -- initialize all lanes
 */
static int
rpmemd_fip_lanes_init(struct rpmemd_fip *fip)
{

	fip->lanes = calloc(fip->nlanes, sizeof(*fip->lanes));
	if (!fip->lanes) {
		RPMEMD_ERR("!allocating lanes");
		goto err_alloc;
	}

	int ret = 0;

	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		ret = rpmemd_fip_lane_init(fip, &fip->lanes[i].base);
		if (ret)
			goto err_lane_init;
	}

	return 0;
err_lane_init:
	for (unsigned j = 0; j < i; j++)
		rpmemd_fip_lane_fini(fip, &fip->lanes[j].base);
	free(fip->lanes);
err_alloc:
	return -1;
}

/*
 * rpmemd_fip_lanes_fini -- deinitialize all lanes
 */
static void
rpmemd_fip_lanes_fini(struct rpmemd_fip *fip)
{
	for (unsigned i = 0; i < fip->nlanes; i++)
		rpmemd_fip_lane_fini(fip, &fip->lanes[i].base);

	free(fip->lanes);
}

/*
 * rpmemd_fip_init_gpspm -- initialize GPSPM resources
 */
static int
rpmemd_fip_init_gpspm(struct rpmemd_fip *fip)
{
	int ret;

	/* allocate persist message buffer */
	size_t msg_size = fip->nlanes * sizeof(struct rpmem_msg_persist);
	fip->pmsg = malloc(msg_size);
	if (!fip->pmsg) {
		RPMEMD_LOG(ERR, "!allocating GPSPM messages buffer");
		goto err_msg_malloc;
	}

	/* register persist message buffer */
	ret = fi_mr_reg(fip->domain, fip->pmsg, msg_size, FI_RECV,
			0, 0, 0, &fip->pmsg_mr, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "registering GPSPM messages buffer");
		goto err_mr_reg_msg;
	}

	/* get persist message buffer's local descriptor */
	fip->pmsg_mr_desc = fi_mr_desc(fip->pmsg_mr);

	/* allocate persist response message buffer */
	size_t msg_resp_size = fip->nlanes *
		sizeof(struct rpmem_msg_persist_resp);
	fip->pres = malloc(msg_resp_size);
	if (!fip->pres) {
		RPMEMD_FI_ERR(ret, "allocating GPSPM messages response buffer");
		goto err_msg_resp_malloc;
	}

	/* register persist response message buffer */
	ret = fi_mr_reg(fip->domain, fip->pres, msg_resp_size, FI_SEND,
			0, 0, 0, &fip->pres_mr, NULL);
	if (ret) {
		RPMEMD_FI_ERR(ret, "registering GPSPM messages "
				"response buffer");
		goto err_mr_reg_msg_resp;
	}

	/* get persist message buffer's local descriptor */
	fip->pres_mr_desc = fi_mr_desc(fip->pres_mr);

	/* initialize lanes */
	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		struct rpmemd_fip_lane *lanep = &fip->lanes[i];

		/* initialize RECV message */
		rpmem_fip_msg_init(&lanep->recv,
				fip->pmsg_mr_desc, 0,
				lanep,
				&fip->pmsg[i],
				sizeof(fip->pmsg[i]),
				FI_COMPLETION);

		/* initialize SEND message */
		rpmem_fip_msg_init(&lanep->send,
				fip->pres_mr_desc, 0,
				lanep,
				&fip->pres[i],
				sizeof(fip->pres[i]),
				FI_COMPLETION);
	}

	return 0;
err_mr_reg_msg_resp:
	free(fip->pres);
err_msg_resp_malloc:
	RPMEMD_FI_CLOSE(fip->pmsg_mr,
			"unregistering GPSPM messages buffer");
err_mr_reg_msg:
	free(fip->pmsg);
err_msg_malloc:
	return -1;
}

/*
 * rpmemd_fip_fini_gpspm -- deinitialize GPSPM resources and return last
 * error code
 */
static int
rpmemd_fip_fini_gpspm(struct rpmemd_fip *fip)
{
	int lret = 0;
	int ret;

	ret = RPMEMD_FI_CLOSE(fip->pmsg_mr,
			"unregistering GPSPM messages buffer");
	if (ret)
		lret = ret;

	ret = RPMEMD_FI_CLOSE(fip->pres_mr,
			"unregistering GPSPM messages response buffer");
	if (ret)
		lret = ret;

	free(fip->pmsg);
	free(fip->pres);

	return lret;
}

/*
 * rpmemd_fip_check_pmsg -- verify persist message
 */
static inline int
rpmemd_fip_check_pmsg(struct rpmemd_fip *fip, struct rpmem_msg_persist *pmsg)
{
	if (pmsg->lane >= fip->nlanes) {
		RPMEMD_LOG(ERR, "invalid lane number -- %lu", pmsg->lane);
		return -1;
	}

	uintptr_t raddr = pmsg->addr;
	uintptr_t laddr = (uintptr_t)fip->addr;

	if (raddr < laddr || raddr + pmsg->size > laddr + fip->size) {
		RPMEMD_LOG(ERR, "invalid address or size requested "
			"for persist operation (0x%lx, %lu)",
			raddr, pmsg->size);
		return -1;
	}

	return 0;
}

/*
 * rpmemd_fip_process_one -- process single persist operation
 */
static int
rpmemd_fip_process_one(struct rpmemd_fip *fip, struct rpmemd_fip_lane *lanep)
{
	int ret = 0;

	/*
	 * Get persist message and persist message response from appropriate
	 * buffers. The persist message is in lane's RECV buffer and the
	 * persist response message in lane's SEND buffer.
	 */
	struct rpmem_msg_persist *pmsg = rpmem_fip_msg_get_pmsg(&lanep->recv);
	struct rpmem_msg_persist_resp *pres =
		rpmem_fip_msg_get_pres(&lanep->send);
	VALGRIND_DO_MAKE_MEM_DEFINED(pmsg, sizeof(*pmsg));

	/* verify persist message */
	ret = rpmemd_fip_check_pmsg(fip, pmsg);
	if (unlikely(ret))
		goto err;

	/* return back the lane id */
	pres->lane = pmsg->lane;

	/*
	 * Perform the persist operation.
	 *
	 * XXX
	 *
	 * Maybe the persist could be divided into flush + drain.
	 * We could issue flush operation, do some other work like
	 * posting RECV buffer and then call drain. Need to consider this.
	 */
	fip->persist((void *)pmsg->addr, pmsg->size);

	/* post lane's RECV buffer */
	ret = rpmemd_fip_gpspm_post_msg(lanep);
	if (unlikely(ret))
		goto err;

	/* post lane's SEND buffer */
	ret = rpmemd_fip_gpspm_post_resp(lanep);

err:
	return ret;
}

/*
 * rpmemd_fip_lane_wait -- wait for specific events on completion queue
 */
static int
rpmemd_fip_lane_wait(struct rpmemd_fip *fip,
	struct rpmemd_fip_lane *lanep, uint64_t e)
{
	struct fi_cq_err_entry err;
	struct fi_cq_msg_entry cq_entry;
	const char *str_err;
	ssize_t sret;
	int ret;

	while (!fip->closing && (lanep->base.event & e)) {
		sret = fi_cq_sread(lanep->base.cq,
				&cq_entry, 1, NULL,
				RPMEM_FIP_CQ_WAIT_MS);

		if (unlikely(fip->closing))
			break;

		if (unlikely(sret == -FI_EAGAIN || sret == 0))
			continue;

		if (unlikely(sret < 0)) {
			ret = (int)sret;
			goto err_cq_read;
		}

		lanep->base.event &= ~cq_entry.flags;
	}

	return 0;
err_cq_read:
	sret = fi_cq_readerr(lanep->base.cq, &err, 0);
	if (sret < 0) {
		RPMEMD_FI_ERR((int)sret, "error reading from completion queue: "
			"cannot read error from completion queue");
		goto err;
	}

	str_err = fi_cq_strerror(lanep->base.cq, err.prov_errno, NULL, NULL, 0);
	RPMEMD_LOG(ERR, "error reading from completion queue: %s", str_err);
err:
	return ret;
}

/*
 * rpmemd_fip_worker -- worker callback which processes persist
 * operation in GPSPM
 */
static void *
rpmemd_fip_worker(void *arg)
{
	struct rpmemd_fip_worker *worker = arg;
	struct rpmemd_fip *fip = worker->fip;
	struct rpmemd_fip_lane *lanep = worker->lanep;
	int ret = 0;

	rpmem_fip_lane_begin(&lanep->base, FI_RECV);

	while (!fip->closing) {
		ret = rpmemd_fip_lane_wait(fip, lanep, FI_SEND|FI_RECV);
		if (ret)
			goto err;

		if (unlikely(fip->closing))
			break;

		ret = rpmemd_fip_process_one(fip, lanep);
		if (ret)
			goto err;

		rpmem_fip_lane_begin(&lanep->base, FI_SEND|FI_RECV);
	}

	return 0;
err:
	return (void *)(uintptr_t)ret;
}

/*
 * rpmemd_fip_process_start_gpspm -- start processing GPSPM messages
 */
static int
rpmemd_fip_process_start_gpspm(struct rpmemd_fip *fip)
{
	fip->workers = malloc(fip->nlanes * sizeof(*fip->workers));
	if (!fip->workers) {
		RPMEMD_LOG(ERR, "!allocating workers");
		goto err_alloc_workers;
	}

	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		fip->workers[i].fip = fip;
		fip->workers[i].lanep = &fip->lanes[i];
		errno = os_thread_create(&fip->workers[i].thread, NULL,
				rpmemd_fip_worker, &fip->workers[i]);
		if (errno) {
			RPMEMD_ERR("!running worker thread");
			goto err_thread_create;
		}
	}

	return 0;
err_thread_create:
	free(fip->workers);
err_alloc_workers:
	return -1;
}

/*
 * rpmemd_fip_process_stop_gpspm -- stop processing GPSPM messages
 */
static int
rpmemd_fip_process_stop_gpspm(struct rpmemd_fip *fip)
{
	/* this stops all worker threads */
	__sync_fetch_and_or(&fip->closing, 1);
	int ret;
	int lret = 0;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		struct rpmemd_fip_worker *worker = &fip->workers[i];
		ret = fi_cq_signal(worker->lanep->base.cq);
		if (ret) {
			RPMEMD_FI_ERR(ret, "sending signal to CQ");
			lret = ret;
		}
		void *tret;
		errno = os_thread_join(worker->thread, &tret);
		if (errno) {
			RPMEMD_LOG(ERR, "!joining cq thread");
			lret = -1;
		} else {
			ret = (int)(uintptr_t)tret;
			if (ret) {
				RPMEMD_LOG(ERR, "cq thread failed with "
					"code -- %d", ret);
				lret = ret;
			}
		}
	}

	free(fip->workers);

	return lret;
}

static struct rpmemd_fip_ops rpmemd_fip_ops[MAX_RPMEM_PM] = {
	[RPMEM_PM_GPSPM] = {
		.init = rpmemd_fip_init_gpspm,
		.fini = rpmemd_fip_fini_gpspm,
		.post = rpmemd_fip_post_gpspm,
		.process_start = rpmemd_fip_process_start_gpspm,
		.process_stop = rpmemd_fip_process_stop_gpspm,
	},
	[RPMEM_PM_APM] = {
		.init = rpmemd_fip_init_apm,
		.fini = rpmemd_fip_fini_apm,
		.post = rpmemd_fip_post_apm,
		.process_start = rpmemd_fip_process_start_apm,
		.process_stop = rpmemd_fip_process_stop_apm,
	},
};

/*
 * rpmemd_fip_set_attr -- save required attributes in rpmemd_fip handle
 */
static void
rpmemd_fip_set_attr(struct rpmemd_fip *fip, struct rpmemd_fip_attr *attr)
{
	fip->addr = attr->addr;
	fip->size = attr->size;
	fip->nthreads = attr->nthreads;
	fip->persist_method = attr->persist_method;
	fip->persist = attr->persist;

	rpmemd_fip_set_nlanes(fip, attr->nlanes);

	fip->cq_size = rpmem_fip_cq_size(fip->persist_method,
			RPMEM_FIP_NODE_SERVER);

	RPMEMD_ASSERT(fip->persist_method < MAX_RPMEM_PM);
	fip->ops = &rpmemd_fip_ops[fip->persist_method];
}

/*
 * rpmemd_fip_init -- initialize fabric provider
 */
struct rpmemd_fip *
rpmemd_fip_init(const char *node, const char *service,
	struct rpmemd_fip_attr *attr, struct rpmem_resp_attr *resp,
	enum rpmem_err *err)
{
	int ret;

	RPMEMD_ASSERT(resp);
	RPMEMD_ASSERT(err);
	RPMEMD_ASSERT(attr);
	RPMEMD_ASSERT(attr->persist);
	RPMEMD_ASSERT(attr->nthreads);

	struct rpmemd_fip *fip = calloc(1, sizeof(*fip));
	if (!fip) {
		RPMEMD_LOG(ERR, "!allocating fabric handle");
		*err = RPMEM_ERR_FATAL;
		return NULL;
	}

	ret = rpmemd_fip_getinfo(fip, service, node, attr->provider);
	if (ret) {
		*err = RPMEM_ERR_BADPROVIDER;
		goto err_getinfo;
	}

	rpmemd_fip_set_attr(fip, attr);

	ret = rpmemd_fip_init_fabric_res(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_fabric_res;
	}

	ret = rpmemd_fip_init_memory(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_memory;
	}

	ret = rpmemd_fip_lanes_init(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init_lanes;
	}

	ret = fip->ops->init(fip);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_init;
	}

	ret = fi_listen(fip->pep);
	if (ret) {
		*err = RPMEM_ERR_FATAL_CONN;
		goto err_fi_listen;
	}

	ret = rpmemd_fip_set_resp(fip, resp);
	if (ret) {
		*err = RPMEM_ERR_FATAL;
		goto err_set_resp;
	}

	return fip;
err_set_resp:
	RPMEMD_FI_CLOSE(fip->pep, "closing passive endpoint");
err_fi_listen:
	fip->ops->fini(fip);
err_init:
	rpmemd_fip_lanes_fini(fip);
err_init_lanes:
	rpmemd_fip_fini_memory(fip);
err_init_memory:
	rpmemd_fip_fini_fabric_res(fip);
err_init_fabric_res:
	fi_freeinfo(fip->fi);
err_getinfo:
	free(fip);
	return NULL;
}

/*
 * rpmemd_fip_fini -- deinitialize fabric provider
 */
void
rpmemd_fip_fini(struct rpmemd_fip *fip)
{
	rpmemd_fip_lanes_fini(fip);
	fip->ops->fini(fip);
	rpmemd_fip_fini_memory(fip);
	rpmemd_fip_fini_fabric_res(fip);
	fi_freeinfo(fip->fi);
	free(fip);
}

/*
 * rpmemd_fip_accept_one -- accept a single connection
 */
static int
rpmemd_fip_accept_one(struct rpmemd_fip *fip,
	struct fi_info *info, struct rpmemd_fip_lane *lanep)
{
	int ret;

	ret = rpmemd_fip_init_ep(fip, info, &lanep->base);
	if (ret)
		goto err_init_ep;

	ret = fip->ops->post(fip, lanep);
	if (ret)
		goto err_post;

	ret = fi_accept(lanep->base.ep, NULL, 0);
	if (ret) {
		RPMEMD_FI_ERR(ret, "accepting connection request");
		goto err_accept;
	}

	fi_freeinfo(info);

	return 0;
err_accept:
err_post:
	rpmemd_fip_fini_ep(&lanep->base);
err_init_ep:
	fi_freeinfo(info);
	return -1;
}

/*
 * rpmemd_fip_accept -- accept a single connection request
 */
int
rpmemd_fip_accept(struct rpmemd_fip *fip, int timeout)
{
	int ret;
	struct fi_eq_cm_entry entry;
	uint32_t event;
	unsigned nreq = 0; /* number of connection requests */
	unsigned ncon = 0; /* number of connected endpoints */
	int connecting = 1;

	while (connecting && (nreq < fip->nlanes || ncon < fip->nlanes)) {
		ret = rpmem_fip_read_eq(fip->eq, &entry,
				&event, timeout);
		if (ret)
			goto err_read_eq;

		switch (event) {
		case FI_CONNREQ:
			ret = rpmemd_fip_accept_one(fip, entry.info,
					&fip->lanes[nreq]);
			if (ret)
				goto err_accept_one;
			nreq++;
			break;
		case FI_CONNECTED:
			ncon++;
			break;
		case FI_SHUTDOWN:
			connecting = 0;
			break;
		default:
			RPMEMD_ERR("unexpected event received (%u)", event);
			goto err_read_eq;

		}
	}

	return 0;
err_accept_one:
err_read_eq:
	return -1;
}

/*
 * rpmemd_fip_wait_close -- wait specified time for connection closed event
 */
int
rpmemd_fip_wait_close(struct rpmemd_fip *fip, int timeout)
{
	struct fi_eq_cm_entry entry;
	int lret = 0;
	uint32_t event;
	int ret;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_read_eq(fip->eq, &entry, &event, timeout);
		if (ret)
			lret = ret;
		if (event != FI_SHUTDOWN) {
			RPMEMD_ERR("unexpected event received "
				"(is %u expected %u)",
				event, FI_SHUTDOWN);
			errno = EINVAL;
			lret = -1;
		}
	}

	return lret;
}

/*
 * rpmemd_fip_close -- close the connection
 */
int
rpmemd_fip_close(struct rpmemd_fip *fip)
{
	int lret = 0;
	int ret;

	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmemd_fip_fini_ep(&fip->lanes[i].base);
		if (ret)
			lret = ret;
	}

	return lret;
}

/*
 * rpmemd_fip_process_start -- start processing
 */
int
rpmemd_fip_process_start(struct rpmemd_fip *fip)
{
	return fip->ops->process_start(fip);
}

/*
 * rpmemd_fip_process_stop -- stop processing
 */
int
rpmemd_fip_process_stop(struct rpmemd_fip *fip)
{
	return fip->ops->process_stop(fip);
}
