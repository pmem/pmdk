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
 * rpmem_fip.c -- rpmem libfabric provider module source file
 */

#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#include <rdma/fabric.h>
#include <rdma/fi_domain.h>
#include <rdma/fi_endpoint.h>
#include <rdma/fi_cm.h>
#include <rdma/fi_errno.h>
#include <rdma/fi_rma.h>

#include "out.h"
#include "util.h"
#include "rpmem_common.h"
#include "rpmem_fip_common.h"
#include "rpmem_proto.h"
#include "rpmem_util.h"
#include "rpmem_fip_msg.h"
#include "rpmem_fip_lane.h"
#include "rpmem_fip.h"

#define RPMEM_FI_ERR(e, fmt, args...)\
	RPMEM_LOG(ERR, fmt ": %s", ## args, fi_strerror((e)))

#define RPMEM_FI_CLOSE(f, fmt, args...) (\
{\
	int ret = fi_close(&(f)->fid);\
	if (ret)\
		RPMEM_FI_ERR(ret, fmt, ## args);\
	ret;\
})

#define RPMEM_RD_BUFF_SIZE 8192

typedef int (*rpmem_fip_persist_fn)(struct rpmem_fip *fip, size_t offset,
		size_t len, unsigned lane);

typedef int (*rpmem_fip_process_fn)(struct rpmem_fip *fip,
		void *context, uint64_t flags);

typedef int (*rpmem_fip_init_fn)(struct rpmem_fip *fip);
typedef void (*rpmem_fip_fini_fn)(struct rpmem_fip *fip);

/*
 * rpmem_fip_ops -- operations specific for persistency method
 */
struct rpmem_fip_ops {
	rpmem_fip_persist_fn persist;
	rpmem_fip_process_fn process;
	rpmem_fip_init_fn lanes_init;
	rpmem_fip_fini_fn lanes_fini;
	rpmem_fip_init_fn lanes_post;
};

/*
 * rpmem_fip_plane_apm -- persist operation's lane for APM
 */
struct rpmem_fip_plane_apm {
	struct rpmem_fip_lane lane;	/* base lane structure */
	struct rpmem_fip_rma write;	/* WRITE message */
	struct rpmem_fip_rma read;	/* READ message */
};

/*
 * rpmem_fip_plane_gpspm -- persist operation's lane for GPSPM
 */
struct rpmem_fip_plane_gpspm {
	struct rpmem_fip_lane lane;	/* base lane structure */
	struct rpmem_fip_rma write;	/* WRITE message */
	struct rpmem_fip_msg send;	/* SEND message */
};

/*
 * rpmem_fip_rlane -- read operation's lane
 */
struct rpmem_fip_rlane {
	struct rpmem_fip_lane lane;	/* base lane structure */
	struct rpmem_fip_rma read;	/* READ message */
};

struct rpmem_fip {
	struct fi_info *fi; /* fabric interface information */
	struct fid_fabric *fabric; /* fabric domain */
	struct fid_domain *domain; /* fabric protection domain */
	struct fid_eq *eq; /* event queue */
	struct fid_ep *ep; /* endpoint */

	volatile int closing;

	struct fid_cq *cq; /* completion queue */
	size_t cq_size;	/* completion queue size */

	uint64_t raddr;	/* remote memory base address */
	uint64_t rkey;	/* remote memory protection key */
	void *laddr;	/* local memory base address */
	size_t size;	/* memory size */
	struct fid_mr *mr; /* local memory region */
	void *mr_desc;	/* local memory descriptor */

	enum rpmem_persist_method persist_method;
	struct rpmem_fip_ops *ops;

	unsigned nlanes;
	union {
		struct rpmem_fip_plane_apm *apm;
		struct rpmem_fip_plane_gpspm *gpspm;
	} lanes;

	struct rpmem_fip_rlane rd_lane; /* lane for read operation */
	void *rd_buff;		/* buffer for read operation */
	struct fid_mr *rd_mr;	/* read buffer memory region */
	void *rd_mr_desc;	/* read buffer memory descriptor */

	struct rpmem_msg_persist *pmsg;	/* persist message buffer */
	struct fid_mr *pmsg_mr;		/* persist message memory region */
	void *pmsg_mr_desc;		/* persist message memory descriptor */

	struct rpmem_msg_persist_resp *pres; /* persist response buffer */
	struct fid_mr *pres_mr;		/* persist response memory region */
	void *pres_mr_desc;	/* persist response memory descriptor */

	struct rpmem_fip_msg *recv;	/* RECV operation messages */

	uint64_t raw_buff;		/* READ-after-WRITE buffer */
	struct fid_mr *raw_mr;		/* RAW memory region */
	void *raw_mr_desc;		/* RAW memory descriptor */

	pthread_t process_thread;	/* processing thread */
};

/*
 * rpmem_fip_set_nlanes -- (internal) set maximum number of lanes supported
 */
static void
rpmem_fip_set_nlanes(struct rpmem_fip *fip, unsigned nlanes)
{
	size_t max_nlanes = rpmem_fip_max_nlanes(fip->fi,
			fip->persist_method, RPMEM_FIP_NODE_CLIENT);

	/*
	 * Get minimum of maximum supported and number of
	 * lanes requested caller.
	 */
	size_t min_nlanes = max_nlanes < nlanes ? max_nlanes : nlanes;

	fip->nlanes = (unsigned)(min_nlanes - 1); /* one for read operation */
}

/*
 * rpmem_fip_getinfo -- (internal) get fabric interface information
 */
static int
rpmem_fip_getinfo(struct rpmem_fip *fip, const char *node, const char *service,
	enum rpmem_provider provider)
{
	int ret = 0;
	struct fi_info *hints = rpmem_fip_get_hints(provider);
	if (!hints) {
		RPMEM_LOG(ERR, "!getting fabric interface information hints");
		goto err_hints;
	}

	ret = fi_getinfo(RPMEM_FIVERSION, node, service,
			0, hints, &fip->fi);
	if (ret) {
		RPMEM_FI_ERR(ret, "getting fabric interface information");
		goto err_fi_getinfo;
	}

	/* fallback to free the hints */
err_fi_getinfo:
	fi_freeinfo(hints);
err_hints:
	return ret;
}

/*
 * rpmem_fip_init_fabric_res -- (internal) initialize common fabric resources
 */
static int
rpmem_fip_init_fabric_res(struct rpmem_fip *fip)
{
	int ret;
	ret = fi_fabric(fip->fi->fabric_attr, &fip->fabric, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening fabric domain");
		goto err_fi_fabric;
	}

	ret = fi_domain(fip->fabric, fip->fi, &fip->domain, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening fabric access domain");
		goto err_fi_domain;
	}

	struct fi_eq_attr eq_attr = {
		.size = 0, /* use default value */
		.flags = 0,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_set = NULL,
	};

	ret = fi_eq_open(fip->fabric, &eq_attr, &fip->eq, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening event queue");
		goto err_eq_open;
	}

	return 0;
err_eq_open:
	RPMEM_FI_CLOSE(fip->domain, "closing fabric access domain");
err_fi_domain:
	RPMEM_FI_CLOSE(fip->fabric, "closing fabric domain");
err_fi_fabric:
	return ret;
}

/*
 * rpmem_fip_fini_fabric_res -- (internal) deinitialize common fabric resources
 */
static void
rpmem_fip_fini_fabric_res(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->eq, "closing event queue");
	RPMEM_FI_CLOSE(fip->domain, "closing fabric access domain");
	RPMEM_FI_CLOSE(fip->fabric, "closing fabric domain");
}

/*
 * rpmem_fip_init_memory -- (internal) initialize common memory resources
 */
static int
rpmem_fip_init_memory(struct rpmem_fip *fip)
{
	int ret;

	/*
	 * Register local memory space. The local memory will be used
	 * with WRITE operation in rpmem_fip_persist function thus
	 * the FI_WRITE access flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->laddr, fip->size,
			FI_WRITE, 0, 0, 0, &fip->mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registrating memory");
		return ret;
	}

	/* get local memory descriptor */
	fip->mr_desc = fi_mr_desc(fip->mr);

	/* allocate buffer for read operation */
	fip->rd_buff = Malloc(RPMEM_RD_BUFF_SIZE);
	if (!fip->rd_buff) {
		RPMEM_LOG(ERR, "!allocating read buffer");
		ret = -1;
		goto err_malloc_rd_buff;
	}

	/*
	 * Register buffer for read operation.
	 * The read operation utilizes READ operation thus
	 * the FI_REMOTE_WRITE flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->rd_buff,
			RPMEM_RD_BUFF_SIZE, FI_REMOTE_WRITE,
			0, 0, 0, &fip->rd_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registrating read buffer");
		goto err_rd_mr;
	}

	/* get read buffer local memory descriptor */
	fip->rd_mr_desc = fi_mr_desc(fip->rd_mr);

	return 0;
err_rd_mr:
	Free(fip->rd_buff);
err_malloc_rd_buff:
	RPMEM_FI_CLOSE(fip->mr, "unregistering memory");
	return ret;
}

/*
 * rpmem_fip_fini_memory -- (internal) deinitialize common memory resources
 */
static void
rpmem_fip_fini_memory(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->rd_mr, "unregistering read buffer");
	Free(fip->rd_buff);
	RPMEM_FI_CLOSE(fip->mr, "unregistering memory");
}

/*
 * rpmem_fip_init_lanes_common -- (internal) initialize common lanes resources
 */
static int
rpmem_fip_init_lanes_common(struct rpmem_fip *fip)
{
	int ret;

	/* initialize lane for read operation */
	ret = rpmem_fip_lane_init(&fip->rd_lane.lane);
	if (ret)
		goto err_lane_init;

	/*
	 * Initialize READ message. The completion is required in order
	 * to signal thread that READ operation has been completed.
	 */
	rpmem_fip_rma_init(&fip->rd_lane.read, fip->rd_mr_desc, 0,
			fip->rkey, &fip->rd_lane, FI_COMPLETION);

	return 0;
err_lane_init:
	return -1;
}

/*
 * rpmem_fip_init_lanes -- (internal) initialize lanes
 */
static int
rpmem_fip_init_lanes(struct rpmem_fip *fip)
{
	int ret;

	ret = rpmem_fip_init_lanes_common(fip);
	if (ret)
		return ret;

	ret = fip->ops->lanes_init(fip);
	if (ret)
		goto err_init_lanes;

	return 0;
err_init_lanes:
	return ret;
}

/*
 * rpmem_fip_init_cq -- (internal) initialize completion queue(s)
 */
static int
rpmem_fip_init_cq(struct rpmem_fip *fip)
{
	int ret;

	struct fi_cq_attr cq_attr = {
		.size = fip->cq_size,
		.flags = 0,
		.format = FI_CQ_FORMAT_MSG,
		.wait_obj = FI_WAIT_UNSPEC,
		.signaling_vector = 0,
		.wait_cond = FI_CQ_COND_NONE,
		.wait_set = NULL,
	};

	ret = fi_cq_open(fip->domain, &cq_attr, &fip->cq, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "opening completion queue");
		goto err_cq_open;
	}

	return 0;
err_cq_open:
	return -1;
}

/*
 * rpmem_fip_fini_cq -- (internal) deinitialize completion queue(s)
 */
static int
rpmem_fip_fini_cq(struct rpmem_fip *fip)
{
	return RPMEM_FI_CLOSE(fip->cq, "closing completion queue");
}

/*
 * rpmem_fip_init_ep -- (internal) initialize endpoint
 */
static int
rpmem_fip_init_ep(struct rpmem_fip *fip)
{
	int ret;

	/* create an endpoint */
	ret = fi_endpoint(fip->domain, fip->fi, &fip->ep, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "allocating endpoint");
		goto err_endpoint;
	}

	/*
	 * Bind an event queue to an endpoint to get
	 * connection-related events for the endpoint.
	 */
	ret = fi_ep_bind(fip->ep, &fip->eq->fid, 0);
	if (ret) {
		RPMEM_FI_ERR(ret, "binding event queue to endpoint");
		goto err_ep_bind_eq;
	}

	/*
	 * Bind a completion queue to an endpoint to get completion
	 * events of specified inbound/outbound operations.
	 *
	 * FI_SELECTIVE_COMPLETION means all inbound/outbound operations
	 * must explicitly specify if the completion event should be
	 * generated or not using FI_COMPLETION flag.
	 *
	 * The completion events received are highly related to the
	 * persistency method used and are configured in lanes
	 * initialization specified for persistency method utilized.
	 */
	ret = fi_ep_bind(fip->ep, &fip->cq->fid,
			FI_RECV | FI_TRANSMIT | FI_SELECTIVE_COMPLETION);
	if (ret) {
		RPMEM_FI_ERR(ret, "binding completion queue to endpoint");
		goto err_ep_bind_cq;
	}

	/*
	 * Enable endpoint so it is possible to post inbound/outbound
	 * operations if required.
	 */
	ret = fi_enable(fip->ep);
	if (ret) {
		RPMEM_FI_ERR(ret, "activating endpoint");
		goto err_fi_enable;
	}

	return 0;
err_fi_enable:
err_ep_bind_cq:
err_ep_bind_eq:
err_endpoint:
	return ret;
}

/*
 * rpmem_fip_fini_ep -- (internal) deinitialize endpoint
 */
static int
rpmem_fip_fini_ep(struct rpmem_fip *fip)
{
	return RPMEM_FI_CLOSE(fip->ep, "closing endpoint");
}

/*
 * rpmem_fip_init_lanes_apm -- (internal) initialize lanes for APM
 */
static int
rpmem_fip_init_lanes_apm(struct rpmem_fip *fip)
{
	int ret;

	/* allocate APM lanes */
	fip->lanes.apm = Zalloc(fip->nlanes * sizeof(*fip->lanes.apm));
	if (!fip->lanes.apm) {
		RPMEM_LOG(ERR, "!allocating APM lanes");
		goto err_malloc_lanes;
	}

	/* register read-after-write buffer */
	ret = fi_mr_reg(fip->domain, &fip->raw_buff, sizeof(fip->raw_buff),
			FI_REMOTE_WRITE, 0, 0, 0, &fip->raw_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registering APM read buffer");
		goto err_fi_raw_mr;
	}

	/* get read-after-write buffer local descriptor */
	fip->raw_mr_desc = fi_mr_desc(fip->raw_mr);

	/*
	 * Initialize all required structures for:
	 * WRITE and READ operations.
	 *
	 * If the completion is required the FI_COMPLETION flag and
	 * appropriate context should be used.
	 *
	 * In APM only the READ completion is required.
	 * The context is a lane structure.
	 */
	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_lane_init(&fip->lanes.apm[i].lane);
		if (ret)
			goto err_lane_init;

		/* WRITE */
		rpmem_fip_rma_init(&fip->lanes.apm[i].write,
				fip->mr_desc, 0,
				fip->rkey,
				&fip->lanes.apm[i],
				0);

		/* READ */
		rpmem_fip_rma_init(&fip->lanes.apm[i].read,
				fip->raw_mr_desc, 0,
				fip->rkey,
				&fip->lanes.apm[i],
				FI_COMPLETION);
	}

	return 0;
err_lane_init:
	for (unsigned j = 0; j < i; j++)
		rpmem_fip_lane_fini(&fip->lanes.apm[i].lane);
err_fi_raw_mr:
	Free(fip->lanes.apm);
err_malloc_lanes:
	return -1;
}

/*
 * rpmem_fip_fini_lanes_apm -- (internal) deinitialize lanes for APM
 */
static void
rpmem_fip_fini_lanes_apm(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->raw_mr, "unregistering APM read buffer");
	Free(fip->lanes.apm);
}

/*
 * rpmem_fip_post_lanes_apm -- (internal) post APM-related buffers
 */
static int
rpmem_fip_post_lanes_apm(struct rpmem_fip *fip)
{
	/* nothing to do */
	return 0;
}

/*
 * rpmem_fip_process_apm -- (internal) process completion queue entry for APM
 */
static int
rpmem_fip_process_apm(struct rpmem_fip *fip, void *context, uint64_t flags)
{
	struct rpmem_fip_lane *lanep = context;

	/* signal READ operation completion */
	rpmem_fip_lane_signal(lanep, flags);

	return 0;
}

/*
 * rpmem_fip_persist_apm -- (internal) perform persist operation for APM
 */
static int
rpmem_fip_persist_apm(struct rpmem_fip *fip, size_t offset,
	size_t len, unsigned lane)
{
	struct rpmem_fip_plane_apm *lanep = &fip->lanes.apm[lane];

	RPMEM_ASSERT(!rpmem_fip_lane_busy(&lanep->lane));

	rpmem_fip_lane_begin(&lanep->lane, FI_READ);

	int ret;
	void *laddr = (void *)((uintptr_t)fip->laddr + offset);
	uint64_t raddr = fip->raddr + offset;

	/* WRITE for requested memory region */
	ret = rpmem_fip_writemsg(fip->ep, &lanep->write, laddr, len, raddr);
	if (unlikely(ret)) {
		RPMEM_FI_ERR((int)ret, "RMA write");
		return ret;
	}

	/* READ to read-after-write buffer */
	ret = rpmem_fip_readmsg(fip->ep, &lanep->read, &fip->raw_buff,
			sizeof(fip->raw_buff), raddr);
	if (unlikely(ret)) {
		RPMEM_FI_ERR((int)ret, "RMA read");
		return (int)ret;
	}

	/* wait for READ completion */
	return rpmem_fip_lane_wait(&lanep->lane, FI_READ);
}

/*
 * rpmem_fip_gpspm_post_resp -- (internal) post persist response message buffer
 */
static inline int
rpmem_fip_gpspm_post_resp(struct rpmem_fip *fip,
	struct rpmem_fip_msg *resp)
{
	int ret = rpmem_fip_recvmsg(fip->ep, resp);
	if (unlikely(ret)) {
		RPMEM_FI_ERR(ret, "posting GPSPM recv buffer");
		return ret;
	}

	return 0;
}

/*
 * rpmem_fip_post_lanes_gpspm -- (internal) post all persist response message
 * buffers
 */
static int
rpmem_fip_post_lanes_gpspm(struct rpmem_fip *fip)
{
	int ret = 0;
	for (unsigned i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_gpspm_post_resp(fip, &fip->recv[i]);
		if (ret)
			break;
	}

	return ret;
}

/*
 * rpmem_fip_init_lanes_gpspm -- (internal) initialize lanes for GPSPM
 */
static int
rpmem_fip_init_lanes_gpspm(struct rpmem_fip *fip)
{
	int ret = 0;

	/* allocate GPSPM lanes */
	fip->lanes.gpspm = Zalloc(fip->nlanes * sizeof(*fip->lanes.gpspm));
	if (!fip->lanes.gpspm) {
		RPMEM_LOG(ERR, "allocating GPSPM lanes");
		goto err_malloc_lanes;
	}

	/* allocate persist messages buffer */
	size_t msg_size = fip->nlanes * sizeof(struct rpmem_msg_persist);
	fip->pmsg = Malloc(msg_size);
	if (!fip->pmsg) {
		RPMEM_LOG(ERR, "!allocating messages buffer");
		ret = -1;
		goto err_malloc_pmsg;
	}

	/*
	 * Register persist messages buffer. The persist messages
	 * are sent to daemon thus the FI_SEND access flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->pmsg, msg_size, FI_SEND,
			0, 0, 0, &fip->pmsg_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registering messages buffer");
		goto err_fi_mr_reg_pmsg;
	}

	/* get persist messages buffer local descriptor */
	fip->pmsg_mr_desc = fi_mr_desc(fip->pmsg_mr);

	/* allocate persist response messages buffer */
	size_t msg_resp_size = fip->nlanes *
				sizeof(struct rpmem_msg_persist_resp);
	fip->pres = Malloc(msg_resp_size);
	if (!fip->pres) {
		RPMEM_LOG(ERR, "!allocating messages response buffer");
		ret = -1;
		goto err_malloc_pres;
	}

	/*
	 * Register persist messages response buffer. The persist response
	 * messages are received from daemon thus the FI_RECV access flag.
	 */
	ret = fi_mr_reg(fip->domain, fip->pres, msg_resp_size, FI_RECV,
			0, 0, 0, &fip->pres_mr, NULL);
	if (ret) {
		RPMEM_FI_ERR(ret, "registering messages response buffer");
		goto err_fi_mr_reg_pres;
	}

	/* get persist response messages buffer local descriptor */
	fip->pres_mr_desc = fi_mr_desc(fip->pres_mr);

	/* allocate RECV structures for fi_recvmsg(3) */
	fip->recv = Malloc(fip->nlanes * sizeof(*fip->recv));
	if (!fip->recv) {
		RPMEM_LOG(ERR, "!allocating response message iov buffer");
		goto err_malloc_recv;
	}

	/*
	 * Initialize all required structures for:
	 * WRITE, SEND and RECV operations.
	 *
	 * If the completion is required the FI_COMPLETION flag and
	 * appropriate context should be used.
	 *
	 * In GPSPM only the RECV and SEND completions are required.
	 *
	 * For RECV the context is RECV operation structure used for
	 * fi_recvmsg(3) function call.
	 *
	 * For SEND the context is lane structure.
	 *
	 * The received buffer contains a lane id which is used
	 * to obtain a lane which must be signaled that operation
	 * has been completed.
	 */
	unsigned i;
	for (i = 0; i < fip->nlanes; i++) {
		ret = rpmem_fip_lane_init(&fip->lanes.gpspm[i].lane);
		if (ret)
			goto err_lane_init;

		/* WRITE */
		rpmem_fip_rma_init(&fip->lanes.gpspm[i].write,
				fip->mr_desc, 0,
				fip->rkey,
				&fip->lanes.gpspm[i],
				0);

		/* SEND */
		rpmem_fip_msg_init(&fip->lanes.gpspm[i].send,
				fip->pmsg_mr_desc, 0,
				&fip->lanes.gpspm[i],
				&fip->pmsg[i],
				sizeof(fip->pmsg[i]),
				FI_COMPLETION);

		/* RECV */
		rpmem_fip_msg_init(&fip->recv[i],
				fip->pres_mr_desc, 0,
				&fip->recv[i],
				&fip->pres[i],
				sizeof(fip->pres[i]),
				FI_COMPLETION);
	}

	return 0;
err_lane_init:
	for (unsigned j = 0; j < i; j++)
		rpmem_fip_lane_fini(&fip->lanes.gpspm[i].lane);
err_malloc_recv:
	RPMEM_FI_CLOSE(fip->pres_mr, "unregistering messages "
			"response buffer");
err_fi_mr_reg_pres:
	Free(fip->pres);
err_malloc_pres:
	RPMEM_FI_CLOSE(fip->pmsg_mr, "unregistering messages buffer");
err_fi_mr_reg_pmsg:
	Free(fip->pmsg);
err_malloc_pmsg:
	Free(fip->lanes.gpspm);
err_malloc_lanes:
	return ret;
}

/*
 * rpmem_fip_fini_lanes_gpspm -- (internal) deinitialize lanes for GPSPM
 */
static void
rpmem_fip_fini_lanes_gpspm(struct rpmem_fip *fip)
{
	RPMEM_FI_CLOSE(fip->pmsg_mr, "unregistering messages buffer");
	RPMEM_FI_CLOSE(fip->pres_mr, "unregistering messages "
			"response buffer");
	Free(fip->pmsg);
	Free(fip->pres);
	Free(fip->recv);
	Free(fip->lanes.gpspm);
}

/*
 * rpmem_fip_process_gpspm -- (internal) process completion queue entry for
 * GPSPM
 */
static int
rpmem_fip_process_gpspm(struct rpmem_fip *fip, void *context, uint64_t flags)
{
	if (flags & FI_RECV) {
		/* RECV completion */
		struct rpmem_fip_msg *resp = context;
		struct rpmem_msg_persist_resp *msg_resp =
			rpmem_fip_msg_get_pres(resp);

		if (unlikely(msg_resp->lane >= fip->nlanes))
			return -1;

		struct rpmem_fip_lane *lanep =
			&fip->lanes.gpspm[msg_resp->lane].lane;

		/* post RECV buffer immediately */
		int ret = rpmem_fip_gpspm_post_resp(fip, resp);
		if (unlikely(ret))
			RPMEM_FI_ERR((int)ret, "MSG send");

		rpmem_fip_lane_sigret(lanep, flags, ret);

		return ret;
	}

	struct rpmem_fip_lane *lanep = context;

	/* SEND completion */
	rpmem_fip_lane_signal(lanep, flags);

	return 0;
}

/*
 * rpmem_fip_persist_gpspm -- (internal) perform persist operation for GPSPM
 */
static int
rpmem_fip_persist_gpspm(struct rpmem_fip *fip, size_t offset,
	size_t len, unsigned lane)
{
	int ret;
	struct rpmem_fip_plane_gpspm *lanep = &fip->lanes.gpspm[lane];

	ret = rpmem_fip_lane_wait(&lanep->lane, FI_SEND);
	if (unlikely(ret)) {
		RPMEM_LOG(ERR, "waiting for SEND buffer");
		return ret;
	}

	RPMEM_ASSERT(!rpmem_fip_lane_busy(&lanep->lane));

	rpmem_fip_lane_begin(&lanep->lane, FI_SEND | FI_RECV);

	void *laddr = (void *)((uintptr_t)fip->laddr + offset);
	uint64_t raddr = fip->raddr + offset;
	struct rpmem_msg_persist *msg;
	struct rpmem_fip_plane_gpspm *gpspm = (void *)lanep;

	/* WRITE for requested memory region */
	ret = rpmem_fip_writemsg(fip->ep, &gpspm->write, laddr, len, raddr);
	if (unlikely(ret)) {
		RPMEM_FI_ERR((int)ret, "RMA write");
		return ret;
	}

	/* SEND persist message */
	msg = rpmem_fip_msg_get_pmsg(&gpspm->send);
	msg->lane = lane;
	msg->addr = raddr;
	msg->size = len;

	ret = rpmem_fip_sendmsg(fip->ep, &gpspm->send);
	if (unlikely(ret)) {
		RPMEM_FI_ERR((int)ret, "MSG send");
		return (int)ret;
	}

	/* wait for persist operation completion */
	return rpmem_fip_lane_wait(&lanep->lane, FI_RECV);
}

/*
 * rpmem_fip_ops -- some operations specific for persistency method used
 */
static struct rpmem_fip_ops rpmem_fip_ops[MAX_RPMEM_PM] = {
	[RPMEM_PM_GPSPM] = {
		.persist = rpmem_fip_persist_gpspm,
		.process = rpmem_fip_process_gpspm,
		.lanes_init = rpmem_fip_init_lanes_gpspm,
		.lanes_fini = rpmem_fip_fini_lanes_gpspm,
		.lanes_post = rpmem_fip_post_lanes_gpspm,
	},
	[RPMEM_PM_APM] = {
		.persist = rpmem_fip_persist_apm,
		.process = rpmem_fip_process_apm,
		.lanes_init = rpmem_fip_init_lanes_apm,
		.lanes_fini = rpmem_fip_fini_lanes_apm,
		.lanes_post = rpmem_fip_post_lanes_apm,
	},
};

/*
 * rpmem_fip_set_attr -- (internal) set required attributes
 */
static void
rpmem_fip_set_attr(struct rpmem_fip *fip, struct rpmem_fip_attr *attr)
{
	fip->raddr = (uint64_t)attr->raddr;
	fip->rkey = attr->rkey;
	fip->laddr = attr->laddr;
	fip->size = attr->size;
	fip->persist_method = attr->persist_method;

	rpmem_fip_set_nlanes(fip, attr->nlanes);

	fip->cq_size = rpmem_fip_cq_size(fip->nlanes,
			fip->persist_method, RPMEM_FIP_NODE_CLIENT);

	fip->ops = &rpmem_fip_ops[fip->persist_method];
}

/*
 * rpmem_fip_signal_all -- (internal) signal all lanes about completion with
 * error code
 */
static void
rpmem_fip_signal_all(struct rpmem_fip *fip, int ret)
{
	switch (fip->persist_method) {
	case RPMEM_PM_APM:
		for (unsigned i = 0; i < fip->nlanes; i++)
			rpmem_fip_lane_sigret(&fip->lanes.apm[i].lane,
					FI_WRITE | FI_READ, ret);
		break;
	case RPMEM_PM_GPSPM:
		for (unsigned i = 0; i < fip->nlanes; i++)
			rpmem_fip_lane_sigret(&fip->lanes.gpspm[i].lane,
					FI_WRITE | FI_SEND | FI_RECV, ret);
		break;
	default:
		RPMEM_ASSERT(0);
	}
}

/*
 * rpmem_fip_process -- (internal) process completion events
 */
static int
rpmem_fip_process(struct rpmem_fip *fip)
{
	ssize_t sret;
	struct fi_cq_err_entry err;
	const char *str_err;
	int ret;
	struct fi_cq_msg_entry *cq_entries;

	cq_entries = Malloc(fip->cq_size * sizeof(*cq_entries));
	if (!cq_entries) {
		RPMEM_LOG(ERR, "!allocating completion queue buffer");
		return -1;
	}

	while (!fip->closing) {
		sret = fi_cq_sread(fip->cq, cq_entries, fip->cq_size,
				NULL, RPMEM_FIP_CQ_WAIT_MS);

		if (unlikely(fip->closing))
			break;

		if (unlikely(sret == -FI_EAGAIN))
			continue;

		if (unlikely(sret < 0)) {
			ret = (int)sret;
			goto err_cq_read;
		}

		for (ssize_t i = 0; i < sret; i++) {
			struct fi_cq_msg_entry *comp = &cq_entries[i];

			/*
			 * If the context is NULL it probably means that
			 * we get an unexpected CQ entry. The CQ is configured
			 * with FI_SELECTIVE_COMPLETION so every inbound or
			 * outbound operation must be issued with FI_COMPLETION
			 * flag and non-NULL context.
			 */
			RPMEM_ASSERT(comp->op_context);

			/* read operation */
			if (unlikely(comp->op_context == &fip->rd_lane)) {
				rpmem_fip_lane_signal(&fip->rd_lane.lane,
						FI_READ);
				continue;
			}

			/* persist operation */
			ret = fip->ops->process(fip, comp->op_context,
					comp->flags);
			if (unlikely(ret))
				goto err;
		}
	}

	Free(cq_entries);
	return 0;
err_cq_read:
	sret = fi_cq_readerr(fip->cq, &err, 0);
	if (sret < 0) {
		RPMEM_FI_ERR((int)sret, "error reading from completion queue: "
			"cannot read error from event queue");
		goto err;
	}

	str_err = fi_cq_strerror(fip->cq, err.prov_errno, NULL, NULL, 0);
	RPMEM_LOG(ERR, "error reading from completion queue: %s", str_err);
err:
	rpmem_fip_signal_all(fip, ret);
	Free(cq_entries);
	return ret;
}

/*
 * rpmem_fip_process_thread -- (internal) process thread callback
 */
static void *
rpmem_fip_process_thread(void *arg)
{
	int ret;

	struct rpmem_fip *fip = arg;

	ret = rpmem_fip_process(fip);

	return (void *)(uintptr_t)ret;
}

/*
 * rpmem_fip_init -- initialize fabric provider
 */
struct rpmem_fip *
rpmem_fip_init(const char *node, const char *service,
	struct rpmem_fip_attr *attr, unsigned *nlanes)
{
	int ret;

	struct rpmem_fip *fip = Zalloc(sizeof(*fip));
	if (!fip) {
		RPMEM_LOG(ERR, "!allocating fabric handle");
		return NULL;
	}

	ret = rpmem_fip_getinfo(fip, node, service, attr->provider);
	if (ret)
		goto err_getinfo;

	rpmem_fip_set_attr(fip, attr);

	*nlanes = fip->nlanes;

	ret = rpmem_fip_init_fabric_res(fip);
	if (ret)
		goto err_init_fabric_res;

	ret = rpmem_fip_init_memory(fip);
	if (ret)
		goto err_init_memory;

	ret = rpmem_fip_init_lanes(fip);
	if (ret)
		goto err_init_lanes;

	return fip;
err_init_lanes:
	rpmem_fip_fini_memory(fip);
err_init_memory:
	rpmem_fip_fini_fabric_res(fip);
err_init_fabric_res:
	fi_freeinfo(fip->fi);
err_getinfo:
	Free(fip);
	return NULL;
}

/*
 * rpmem_fip_fini -- deinitialize fabric provider
 */
void
rpmem_fip_fini(struct rpmem_fip *fip)
{
	fip->ops->lanes_fini(fip);
	rpmem_fip_fini_memory(fip);
	rpmem_fip_fini_fabric_res(fip);
	fi_freeinfo(fip->fi);
	Free(fip);
}

/*
 * rpmem_fip_connect -- connect to remote peer
 */
int
rpmem_fip_connect(struct rpmem_fip *fip)
{
	int ret;
	struct fi_eq_cm_entry entry;

	ret = rpmem_fip_init_cq(fip);
	if (ret)
		goto err_init_cq;

	ret = rpmem_fip_init_ep(fip);
	if (ret)
		goto err_init_ep;

	ret = fip->ops->lanes_post(fip);
	if (ret)
		goto err_lanes_post;

	ret = fi_connect(fip->ep, fip->fi->dest_addr, NULL, 0);
	if (ret) {
		RPMEM_FI_ERR(ret, "initiating connection request");
		goto err_fi_connect;
	}

	ret = rpmem_fip_read_eq(fip->eq, &entry, FI_CONNECTED,
			&fip->ep->fid, -1);
	if (ret)
		goto err_fi_eq_read;

	return 0;
err_fi_eq_read:
err_fi_connect:
err_lanes_post:
	rpmem_fip_fini_ep(fip);
err_init_ep:
	rpmem_fip_fini_cq(fip);
err_init_cq:
	return ret;
}

/*
 * rpmem_fip_close -- close connection to remote peer
 */
int
rpmem_fip_close(struct rpmem_fip *fip)
{
	int ret;
	int lret = 0;

	ret = fi_shutdown(fip->ep, 0);
	if (ret) {
		RPMEM_FI_ERR(ret, "disconnecting endpoint");
		lret = ret;
	}

	ret = rpmem_fip_fini_ep(fip);
	if (ret)
		lret = ret;

	ret = rpmem_fip_fini_cq(fip);
	if (ret)
		lret = ret;

	return lret;
}

/*
 * prmem_fip_process -- run process thread
 */
int
rpmem_fip_process_start(struct rpmem_fip *fip)
{
	int ret;

	ret = pthread_create(&fip->process_thread, NULL,
			rpmem_fip_process_thread, fip);
	if (ret) {
		RPMEM_LOG(ERR, "creating process thread -- %d", ret);
	}

	return ret;
}

/*
 * rpmem_fip_process_stop -- stop process thread
 */
int
rpmem_fip_process_stop(struct rpmem_fip *fip)
{
	int ret;

	fip->closing = 1;

	void *tret;
	ret = pthread_join(fip->process_thread, &tret);
	if (ret) {
		RPMEM_LOG(ERR, "joining process thread -- %d", ret);
		return ret;
	} else {
		ret = (int)(uintptr_t)tret;
		if (ret) {
			RPMEM_LOG(ERR, "process thread failed -- %d", ret);
		}
	}

	return ret;
}

/*
 * rpmem_fip_persist -- perform remote persist operation
 */
int
rpmem_fip_persist(struct rpmem_fip *fip, size_t offset, size_t len,
	unsigned lane)
{
	RPMEM_ASSERT(lane < fip->nlanes);
	if (unlikely(lane >= fip->nlanes)) {
		errno = EINVAL;
		return -1;
	}

	return fip->ops->persist(fip, offset, len, lane);
}

/*
 * rpmem_fip_read -- perform read operation
 */
int
rpmem_fip_read(struct rpmem_fip *fip, void *buff, size_t len, size_t off)
{
	RPMEM_ASSERT(!rpmem_fip_lane_busy(&fip->rd_lane.lane));

	int ret = 0;
	size_t rd = 0;
	uint8_t *cbuff = buff;
	while (rd < len) {
		rpmem_fip_lane_begin(&fip->rd_lane.lane, FI_READ);

		size_t rd_len = len - rd < RPMEM_RD_BUFF_SIZE ?
				len - rd : RPMEM_RD_BUFF_SIZE;
		size_t rd_off = off + rd;
		uint64_t raddr = fip->raddr + rd_off;

		ret = rpmem_fip_readmsg(fip->ep, &fip->rd_lane.read,
				fip->rd_buff, rd_len, raddr);

		ret = rpmem_fip_lane_wait(&fip->rd_lane.lane, FI_READ);
		if (ret)
			return ret;

		memcpy(&cbuff[rd], fip->rd_buff, rd_len);

		rd += rd_len;
	}

	return ret;
}

/*
 * rpmem_fip_monitor -- monitor connection state
 */
int
rpmem_fip_monitor(struct rpmem_fip *fip, int nonblock)
{
	struct fi_eq_cm_entry entry;
	int timeout = nonblock ? 0 : -1;
	return rpmem_fip_read_eq(fip->eq, &entry, FI_CONNECTED,
			&fip->ep->fid, timeout);
}
